/* mbed Microcontroller Library
 * Copyright (c) 2017 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include <events/mbed_events.h>
#include <mbed.h>
#include "ble/BLE.h"
#include "ble/Gap.h"
#include "ble/GattServer.h"
#include "ble/services/BatteryService.h"
#include "ble/services/DeviceInformationService.h"

#include "pretty_printer.h"

const static char DEVICE_NAME[] = "PlantMonitorSensors";
const static char MFG_NAME[] = "PlantmonCo";
const static char MODEL_NUMBER[] = "PlantmonGen1";
const static char SERIAL_NUMBER[] = "ABC123";
const static char HW_REV[] = "0.1.0";
const static char FW_REV[] = "0.1.0";
const static char SW_REV[] = "0.0.0";

static EventQueue event_queue(/* event count */ 32 * EVENTS_EVENT_SIZE);

// TODO: complete PlantEnvironmentService class
// chars: temp, humidity, light, soil moisture
class PlantEnvironmentService {
public:
    typedef int16_t  TemperatureType_t;
    typedef uint16_t HumidityType_t;
    typedef uint8_t MoistureType_t;
    typedef uint32_t LightType_t;

    const static uint16_t PLANT_ENV_SERVICE_UUID = 0x0;

    PlantEnvironmentService(BLE &ble) : _ble(ble) {

    }
private:
    BLE &_ble;

    TemperatureType_t _temperature;
    HumidityType_t _humidity;
    MoistureType_t _soil_moisture;
    LightType_t _ambient_light;

};

class PlantMonitor : ble::Gap::EventHandler {
public:
    PlantMonitor(BLE &ble, events::EventQueue &event_queue) : 
    _ble(ble),
    _event_queue(event_queue),
    _battery_uuid(GattService::UUID_BATTERY_SERVICE),
    _battery_level(50),
    _battery_service(ble, _battery_level),
    _device_info_uuid(GattService::UUID_DEVICE_INFORMATION_SERVICE),
    _device_info_service(ble, MFG_NAME, MODEL_NUMBER, SERIAL_NUMBER, HW_REV, FW_REV, SW_REV),
    _adv_data_builder(_adv_buffer) {

    }
    
    void start() {
        _ble.gap().setEventHandler(this);

        _ble.init(this, &PlantMonitor::on_init_complete);

        _event_queue.call_every(1000, this, &PlantMonitor::update_sensor_value);

        _event_queue.dispatch_forever();
    }
private:
    /** Callback triggered when the ble initialization process has finished */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *params) {
        if (params->error != BLE_ERROR_NONE) {
            printf("Ble initialization failed.");
            return;
        }

        print_mac_address();

        start_advertising();
    }
    
    void start_advertising() {
        /* Create advertising parameters and payload */

        ble::AdvertisingParameters adv_parameters(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(1000))
        );

        UUID all_uuids[2] = {_battery_uuid, _device_info_uuid};
        _adv_data_builder.setFlags();
        _adv_data_builder.setLocalServiceList(mbed::make_Span(all_uuids, 2));
        _adv_data_builder.setName(DEVICE_NAME);

        /* Setup advertising */

        ble_error_t error = _ble.gap().setAdvertisingParameters(
            ble::LEGACY_ADVERTISING_HANDLE,
            adv_parameters
        );

        if (error) {
            printf("_ble.gap().setAdvertisingParameters() failed\r\n");
            return;
        }

        error = _ble.gap().setAdvertisingPayload(
            ble::LEGACY_ADVERTISING_HANDLE,
            _adv_data_builder.getAdvertisingData()
        );

        if (error) {
            printf("_ble.gap().setAdvertisingPayload() failed\r\n");
            return;
        }

        /* Start advertising */

        error = _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

        if (error) {
            printf("_ble.gap().startAdvertising() failed\r\n");
            return;
        }
    }
    
    /* Event handler */
    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent&) {
        _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
    }
    
   virtual void onConnectionComplete(const ble::ConnectionCompleteEvent &event) {
        if (event.getStatus() == BLE_ERROR_NONE) {
            _connected = true;
            // TODO: set LED when connected. Is multiple connections possible?
        }
    }
    
    void update_sensor_value() {
        if (_ble.gap().getState().connected) {
            _battery_level++;
            if (_battery_level > 100) {
                _battery_level = 20;
            }

            _battery_service.updateBatteryLevel(_battery_level);
        }
    }

    BLE &_ble;
    events::EventQueue &_event_queue;
    
    bool _connected;

    UUID _battery_uuid;
    uint8_t _battery_level;
    BatteryService _battery_service;

    UUID _device_info_uuid;
    DeviceInformationService _device_info_service;

    uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;

};

/** Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

int main() {
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);

    PlantMonitor app(ble, event_queue);
    app.start();

    return 0;
}
