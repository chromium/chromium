// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_message_handler.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/ash/system/fake_input_device_settings.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_dialog.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"

namespace {

// Define the name of the callback functions that will be used by JavaScript.
const char kInitialize[] = "initializeDeviceEmulator";
const char kBluetoothDiscoverFunction[] = "requestBluetoothDiscover";
const char kBluetoothPairFunction[] = "requestBluetoothPair";
const char kRequestBluetoothInfo[] = "requestBluetoothInfo";
const char kRequestPowerInfo[] = "requestPowerInfo";
const char kRequestAudioNodes[] = "requestAudioNodes";

// Define update function that will update the state of the audio ui.
const char kInsertAudioNode[] = "insertAudioNode";
const char kRemoveAudioNode[] = "removeAudioNode";

// Define update functions that will update the power properties to the
// variables defined in the web UI.
const char kRemoveBluetoothDevice[] = "removeBluetoothDevice";
const char kUpdateBatteryPercent[] = "updateBatteryPercent";
const char kUpdateBatteryState[] = "updateBatteryState";
const char kUpdateTimeToEmpty[] = "updateTimeToEmpty";
const char kUpdateTimeToFull[] = "updateTimeToFull";
const char kUpdatePowerSources[] = "updatePowerSources";
const char kUpdatePowerSourceId[] = "updatePowerSourceId";
const char kSetHasTouchpad[] = "setHasTouchpad";
const char kSetHasMouse[] = "setHasMouse";

const char kPairedPropertyName[] = "Paired";
const char kAudioNodesUpdated[] = "audioNodesUpdated";

// Wattages to use as max power for power sources.
const double kPowerLevelHigh = 50;
const double kPowerLevelLow = 2;

bool GetString(const base::Value& dict,
               base::StringPiece key,
               std::string* result) {
  CHECK(result);
  const std::string* value = dict.GetDict().FindString(key);
  if (value) {
    *result = *value;
  }
  return value;
}

}  // namespace

namespace chromeos {

class DeviceEmulatorMessageHandler::BluetoothObserver
    : public bluez::BluetoothDeviceClient::Observer {
 public:
  explicit BluetoothObserver(DeviceEmulatorMessageHandler* owner)
      : owner_(owner) {
    owner_->fake_bluetooth_device_client_->AddObserver(this);
  }

  BluetoothObserver(const BluetoothObserver&) = delete;
  BluetoothObserver& operator=(const BluetoothObserver&) = delete;

  ~BluetoothObserver() override {
    owner_->fake_bluetooth_device_client_->RemoveObserver(this);
  }

  // chromeos::BluetoothDeviceClient::Observer.
  void DeviceAdded(const dbus::ObjectPath& object_path) override;

  // chromeos::BluetoothDeviceClient::Observer.
  void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name) override;

  // chromeos::BluetoothDeviceClient::Observer.
  void DeviceRemoved(const dbus::ObjectPath& object_path) override;

 private:
  DeviceEmulatorMessageHandler* owner_;
};

void DeviceEmulatorMessageHandler::BluetoothObserver::DeviceAdded(
    const dbus::ObjectPath& object_path) {
  base::Value device = owner_->GetDeviceInfo(object_path);

  // Request to add the device to the view's list of devices.
  owner_->FireWebUIListener("bluetooth-device-added", device);
}

void DeviceEmulatorMessageHandler::BluetoothObserver::DevicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (property_name == kPairedPropertyName) {
    owner_->FireWebUIListener("device-paired-from-tray",
                              base::Value(object_path.value()));
  }
}

void DeviceEmulatorMessageHandler::BluetoothObserver::DeviceRemoved(
    const dbus::ObjectPath& object_path) {
  owner_->FireWebUIListener("device-removed-from-main-adapter",
                            base::Value(object_path.value()));
}

class DeviceEmulatorMessageHandler::CrasAudioObserver
    : public CrasAudioClient::Observer {
 public:
  explicit CrasAudioObserver(DeviceEmulatorMessageHandler* owner)
      : owner_(owner) {
    chromeos::FakeCrasAudioClient::Get()->AddObserver(this);
  }

  CrasAudioObserver(const CrasAudioObserver&) = delete;
  CrasAudioObserver& operator=(const CrasAudioObserver&) = delete;

  ~CrasAudioObserver() override {
    chromeos::FakeCrasAudioClient::Get()->RemoveObserver(this);
  }

  // chromeos::CrasAudioClient::Observer.
  void NodesChanged() override {
    if (!owner_->IsJavascriptAllowed()) {
      return;
    }
    owner_->UpdateAudioNodes();
  }

 private:
  DeviceEmulatorMessageHandler* owner_;
};

class DeviceEmulatorMessageHandler::PowerObserver
    : public PowerManagerClient::Observer {
 public:
  explicit PowerObserver(DeviceEmulatorMessageHandler* owner) : owner_(owner) {
    owner_->fake_power_manager_client_->AddObserver(this);
  }

  PowerObserver(const PowerObserver&) = delete;
  PowerObserver& operator=(const PowerObserver&) = delete;

  ~PowerObserver() override {
    owner_->fake_power_manager_client_->RemoveObserver(this);
  }

  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

 private:
  DeviceEmulatorMessageHandler* owner_;
};

void DeviceEmulatorMessageHandler::PowerObserver::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  base::DictionaryValue power_properties;

  power_properties.GetDict().Set("battery_percent",
                                 int(proto.battery_percent()));
  power_properties.GetDict().Set("battery_state", int(proto.battery_state()));
  power_properties.GetDict().Set("external_power", int(proto.external_power()));
  power_properties.GetDict().Set("battery_time_to_empty_sec",
                                 int(proto.battery_time_to_empty_sec()));
  power_properties.GetDict().Set("battery_time_to_full_sec",
                                 int(proto.battery_time_to_full_sec()));
  power_properties.GetDict().Set("external_power_source_id",
                                 proto.external_power_source_id());

  owner_->FireWebUIListener("power-properties-updated", power_properties);
}

DeviceEmulatorMessageHandler::DeviceEmulatorMessageHandler()
    : fake_bluetooth_device_client_(
          static_cast<bluez::FakeBluetoothDeviceClient*>(
              bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient())),
      fake_power_manager_client_(chromeos::FakePowerManagerClient::Get()) {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&DeviceEmulatorMessageHandler::BluetoothDeviceAdapterReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

DeviceEmulatorMessageHandler::~DeviceEmulatorMessageHandler() = default;

void DeviceEmulatorMessageHandler::Init(const base::Value::List& args) {
  AllowJavascript();
}

void DeviceEmulatorMessageHandler::BluetoothDeviceAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter) {
    LOG(ERROR) << "Bluetooth adapter not available";
    return;
  }
  bluetooth_adapter_ = adapter;
}

void DeviceEmulatorMessageHandler::RequestPowerInfo(
    const base::Value::List& args) {
  fake_power_manager_client_->RequestStatusUpdate();
}

void DeviceEmulatorMessageHandler::HandleRemoveBluetoothDevice(
    const base::Value::List& args) {
  CHECK(!args.empty());
  std::string path = args[0].GetString();
  fake_bluetooth_device_client_->RemoveDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(path));
}

void DeviceEmulatorMessageHandler::HandleRequestBluetoothDiscover(
    const base::Value::List& args) {
  CreateBluetoothDeviceFromListValue(args);
}

void DeviceEmulatorMessageHandler::HandleRequestBluetoothInfo(
    const base::Value::List& args) {
  AllowJavascript();
  // Get a list containing paths of the devices which are connected to
  // the main adapter.
  std::vector<dbus::ObjectPath> paths =
      fake_bluetooth_device_client_->GetDevicesForAdapter(
          dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath));

  // Get each device's properties.
  base::Value devices(base::Value::Type::LIST);
  for (const dbus::ObjectPath& path : paths)
    devices.Append(GetDeviceInfo(path));

  base::Value predefined_devices =
      fake_bluetooth_device_client_->GetBluetoothDevicesAsDictionaries();

  base::Value pairing_method_options(base::Value::Type::LIST);
  pairing_method_options.Append(
      bluez::FakeBluetoothDeviceClient::kPairingMethodNone);
  pairing_method_options.Append(
      bluez::FakeBluetoothDeviceClient::kPairingMethodPinCode);
  pairing_method_options.Append(
      bluez::FakeBluetoothDeviceClient::kPairingMethodPassKey);

  base::Value pairing_action_options(base::Value::Type::LIST);
  pairing_action_options.Append(
      bluez::FakeBluetoothDeviceClient::kPairingActionDisplay);
  pairing_action_options.Append(
      bluez::FakeBluetoothDeviceClient::kPairingActionRequest);
  pairing_action_options.Append(
      bluez::FakeBluetoothDeviceClient::kPairingActionConfirmation);
  pairing_action_options.Append(
      bluez::FakeBluetoothDeviceClient::kPairingActionFail);

  base::Value info(base::Value::Type::DICTIONARY);
  info.GetDict().Set("predefined_devices", std::move(predefined_devices));
  info.GetDict().Set("devices", std::move(devices));
  info.GetDict().Set("pairing_method_options",
                     std::move(pairing_method_options));
  info.GetDict().Set("pairing_action_options",
                     std::move(pairing_action_options));

  // Send the list of devices to the view.
  FireWebUIListener("bluetooth-info-updated", info);
}

void DeviceEmulatorMessageHandler::HandleRequestBluetoothPair(
    const base::Value::List& args) {
  // Create the device if it does not already exist.
  std::string path = CreateBluetoothDeviceFromListValue(args);
  bluez::FakeBluetoothDeviceClient::Properties* props =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(path));

  // Try to pair the device with the main adapter. The device is identified
  // by its device ID, which, in this case is the same as its address.
  ConnectToBluetoothDevice(props->address.value());
  if (!props->paired.value()) {
    FireWebUIListener("pair-failed", base::Value(path));
  }
}

void DeviceEmulatorMessageHandler::HandleRequestAudioNodes(
    const base::Value::List& args) {
  AllowJavascript();
  UpdateAudioNodes();
}

void DeviceEmulatorMessageHandler::HandleInsertAudioNode(
    const base::Value::List& args) {
  AudioNode audio_node;

  const base::Value& device_value = args[0];
  CHECK(device_value.is_dict());
  const base::DictionaryValue& device_dict =
      base::Value::AsDictionaryValue(device_value);
  audio_node.is_input = device_dict.GetDict().FindBool("isInput").value();
  CHECK(GetString(device_dict, "deviceName", &audio_node.device_name));
  CHECK(GetString(device_dict, "type", &audio_node.type));
  CHECK(GetString(device_dict, "name", &audio_node.name));
  audio_node.active = device_dict.GetDict().FindBool("active").value();

  std::string tmp_id;
  CHECK(GetString(device_dict, "id", &tmp_id));
  CHECK(base::StringToUint64(tmp_id, &audio_node.id));

  chromeos::FakeCrasAudioClient::Get()->InsertAudioNodeToList(audio_node);
}

void DeviceEmulatorMessageHandler::HandleRemoveAudioNode(
    const base::Value::List& args) {
  CHECK(!args.empty());
  std::string tmp_id = args[0].GetString();
  uint64_t id;
  CHECK(base::StringToUint64(tmp_id, &id));

  chromeos::FakeCrasAudioClient::Get()->RemoveAudioNodeFromList(id);
}

void DeviceEmulatorMessageHandler::HandleSetHasTouchpad(
    const base::Value::List& args) {
  CHECK(!args.empty());
  const bool has_touchpad = args[0].GetBool();

  system::InputDeviceSettings::Get()->GetFakeInterface()->set_touchpad_exists(
      has_touchpad);
}

void DeviceEmulatorMessageHandler::HandleSetHasMouse(
    const base::Value::List& args) {
  CHECK(!args.empty());
  const bool has_mouse = args[0].GetBool();

  system::InputDeviceSettings::Get()->GetFakeInterface()->set_mouse_exists(
      has_mouse);
}

void DeviceEmulatorMessageHandler::UpdateBatteryPercent(
    const base::Value::List& args) {
  if (args.size() >= 1 && args[0].is_int()) {
    int new_percent = args[0].GetInt();
    power_manager::PowerSupplyProperties props =
        *fake_power_manager_client_->GetLastStatus();
    props.set_battery_percent(new_percent);
    fake_power_manager_client_->UpdatePowerProperties(props);
  }
}

void DeviceEmulatorMessageHandler::UpdateBatteryState(
    const base::Value::List& args) {
  if (args.size() >= 1 && args[0].is_int()) {
    int battery_state = args[0].GetInt();
    power_manager::PowerSupplyProperties props =
        *fake_power_manager_client_->GetLastStatus();
    props.set_battery_state(
        static_cast<power_manager::PowerSupplyProperties_BatteryState>(
            battery_state));
    fake_power_manager_client_->UpdatePowerProperties(props);
  }
}

void DeviceEmulatorMessageHandler::UpdateTimeToEmpty(
    const base::Value::List& args) {
  if (args.size() >= 1 && args[0].is_int()) {
    int new_time = args[0].GetInt();
    power_manager::PowerSupplyProperties props =
        *fake_power_manager_client_->GetLastStatus();
    props.set_battery_time_to_empty_sec(new_time);
    fake_power_manager_client_->UpdatePowerProperties(props);
  }
}

void DeviceEmulatorMessageHandler::UpdateTimeToFull(
    const base::Value::List& args) {
  if (args.size() >= 1 && args[0].is_int()) {
    int new_time = args[0].GetInt();
    power_manager::PowerSupplyProperties props =
        *fake_power_manager_client_->GetLastStatus();
    props.set_battery_time_to_full_sec(new_time);
    fake_power_manager_client_->UpdatePowerProperties(props);
  }
}

void DeviceEmulatorMessageHandler::UpdatePowerSources(
    const base::Value::List& args) {
  CHECK(!args.empty() && args[0].is_list());
  base::Value::ConstListView sources = args[0].GetListDeprecated();

  power_manager::PowerSupplyProperties props =
      *fake_power_manager_client_->GetLastStatus();

  std::string selected_id = props.external_power_source_id();

  props.clear_available_external_power_source();
  props.set_external_power_source_id("");

  // Try to find the previously selected source in the list.
  const power_manager::PowerSupplyProperties_PowerSource* selected_source =
      nullptr;
  for (const auto& val : sources) {
    CHECK(val.is_dict());
    power_manager::PowerSupplyProperties_PowerSource* source =
        props.add_available_external_power_source();
    const std::string* id = val.GetDict().FindString("id");
    CHECK(id);
    source->set_id(*id);
    const std::string* device_type = val.GetDict().FindString("type");
    CHECK(device_type);
    bool dual_role = *device_type == "DualRoleUSB";
    source->set_active_by_default(!dual_role);
    if (dual_role)
      props.set_supports_dual_role_devices(true);
    absl::optional<int> port = val.GetDict().FindInt("port");
    CHECK(port.has_value());
    source->set_port(
        static_cast<power_manager::PowerSupplyProperties_PowerSource_Port>(
            port.value()));
    const std::string* power_level = val.GetDict().FindString("power");
    CHECK(power_level);
    source->set_max_power(*power_level == "high" ? kPowerLevelHigh
                                                 : kPowerLevelLow);
    if (*id == selected_id)
      selected_source = source;
  }

  // Emulate the device's source selection process.
  for (const auto& source : props.available_external_power_source()) {
    if (!source.active_by_default())
      continue;
    if (selected_source && selected_source->active_by_default() &&
        source.max_power() < selected_source->max_power()) {
      continue;
    }
    selected_source = &source;
  }

  fake_power_manager_client_->UpdatePowerProperties(props);
  fake_power_manager_client_->SetPowerSource(
      selected_source ? selected_source->id() : "");
}

void DeviceEmulatorMessageHandler::UpdatePowerSourceId(
    const base::Value::List& args) {
  CHECK(!args.empty() && args[0].is_string());
  std::string id = args[0].GetString();
  fake_power_manager_client_->SetPowerSource(id);
}

void DeviceEmulatorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kInitialize, base::BindRepeating(&DeviceEmulatorMessageHandler::Init,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRequestPowerInfo,
      base::BindRepeating(&DeviceEmulatorMessageHandler::RequestPowerInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateBatteryPercent,
      base::BindRepeating(&DeviceEmulatorMessageHandler::UpdateBatteryPercent,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateBatteryState,
      base::BindRepeating(&DeviceEmulatorMessageHandler::UpdateBatteryState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateTimeToEmpty,
      base::BindRepeating(&DeviceEmulatorMessageHandler::UpdateTimeToEmpty,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateTimeToFull,
      base::BindRepeating(&DeviceEmulatorMessageHandler::UpdateTimeToFull,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdatePowerSources,
      base::BindRepeating(&DeviceEmulatorMessageHandler::UpdatePowerSources,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdatePowerSourceId,
      base::BindRepeating(&DeviceEmulatorMessageHandler::UpdatePowerSourceId,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRequestAudioNodes,
      base::BindRepeating(
          &DeviceEmulatorMessageHandler::HandleRequestAudioNodes,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kInsertAudioNode,
      base::BindRepeating(&DeviceEmulatorMessageHandler::HandleInsertAudioNode,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRemoveAudioNode,
      base::BindRepeating(&DeviceEmulatorMessageHandler::HandleRemoveAudioNode,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kBluetoothDiscoverFunction,
      base::BindRepeating(
          &DeviceEmulatorMessageHandler::HandleRequestBluetoothDiscover,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kBluetoothPairFunction,
      base::BindRepeating(
          &DeviceEmulatorMessageHandler::HandleRequestBluetoothPair,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRequestBluetoothInfo,
      base::BindRepeating(
          &DeviceEmulatorMessageHandler::HandleRequestBluetoothInfo,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRemoveBluetoothDevice,
      base::BindRepeating(
          &DeviceEmulatorMessageHandler::HandleRemoveBluetoothDevice,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kSetHasTouchpad,
      base::BindRepeating(&DeviceEmulatorMessageHandler::HandleSetHasTouchpad,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kSetHasMouse,
      base::BindRepeating(&DeviceEmulatorMessageHandler::HandleSetHasMouse,
                          base::Unretained(this)));
}

void DeviceEmulatorMessageHandler::OnJavascriptAllowed() {
  bluetooth_observer_ = std::make_unique<BluetoothObserver>(this);
  cras_audio_observer_ = std::make_unique<CrasAudioObserver>(this);
  power_observer_ = std::make_unique<PowerObserver>(this);

  system::InputDeviceSettings::Get()->TouchpadExists(
      base::BindOnce(&DeviceEmulatorMessageHandler::TouchpadExists,
                     weak_ptr_factory_.GetWeakPtr()));
  system::InputDeviceSettings::Get()->MouseExists(
      base::BindOnce(&DeviceEmulatorMessageHandler::MouseExists,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceEmulatorMessageHandler::OnJavascriptDisallowed() {
  bluetooth_observer_.reset();
  cras_audio_observer_.reset();
  power_observer_.reset();
}

std::string DeviceEmulatorMessageHandler::CreateBluetoothDeviceFromListValue(
    const base::Value::List& args) {
  bluez::FakeBluetoothDeviceClient::IncomingDeviceProperties props;

  const base::Value& device_value = args[0];
  CHECK(device_value.is_dict());
  const base::DictionaryValue& device_dict =
      base::Value::AsDictionaryValue(device_value);
  CHECK(GetString(device_dict, "path", &props.device_path));
  CHECK(GetString(device_dict, "name", &props.device_name));
  CHECK(GetString(device_dict, "alias", &props.device_alias));
  CHECK(GetString(device_dict, "address", &props.device_address));
  CHECK(GetString(device_dict, "pairingMethod", &props.pairing_method));
  CHECK(GetString(device_dict, "pairingAuthToken", &props.pairing_auth_token));
  CHECK(GetString(device_dict, "pairingAction", &props.pairing_action));

  absl::optional<int> class_value = device_dict.GetDict().FindInt("classValue");
  CHECK(class_value);
  props.device_class = *class_value;

  props.is_trusted = device_dict.GetDict().FindBool("isTrusted").value();
  props.incoming = device_dict.GetDict().FindBool("incoming").value();

  // Create the device and store it in the FakeBluetoothDeviceClient's observed
  // list of devices.
  fake_bluetooth_device_client_->CreateDeviceWithProperties(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath), props);

  return props.device_path;
}

base::Value DeviceEmulatorMessageHandler::GetDeviceInfo(
    const dbus::ObjectPath& object_path) {
  // Get the device's properties.
  bluez::FakeBluetoothDeviceClient::Properties* props =
      fake_bluetooth_device_client_->GetProperties(object_path);
  bluez::FakeBluetoothDeviceClient::SimulatedPairingOptions* options =
      fake_bluetooth_device_client_->GetPairingOptions(object_path);

  base::Value device(base::Value::Type::DICTIONARY);
  device.GetDict().Set("path", object_path.value());
  device.GetDict().Set("name", props->name.value());
  device.GetDict().Set("alias", props->alias.value());
  device.GetDict().Set("address", props->address.value());
  if (options) {
    device.GetDict().Set("pairingMethod", options->pairing_method);
    device.GetDict().Set("pairingAuthToken", options->pairing_auth_token);
    device.GetDict().Set("pairingAction", options->pairing_action);
  } else {
    device.GetDict().Set("pairingMethod", "");
    device.GetDict().Set("pairingAuthToken", "");
    device.GetDict().Set("pairingAction", "");
  }
  device.GetDict().Set("classValue", int(props->bluetooth_class.value()));
  device.GetDict().Set("isTrusted", bool(props->trusted.value()));
  device.GetDict().Set("incoming", false);

  base::Value uuids(base::Value::Type::LIST);
  for (const std::string& uuid : props->uuids.value())
    uuids.Append(uuid);
  device.GetDict().Set("uuids", std::move(uuids));

  return device;
}

void DeviceEmulatorMessageHandler::ConnectToBluetoothDevice(
    const std::string& address) {
  if (!bluetooth_adapter_) {
    LOG(ERROR) << "Bluetooth adapter not ready";
    return;
  }
  device::BluetoothDevice* device = bluetooth_adapter_->GetDevice(address);
  if (!device || device->IsConnecting() ||
      (device->IsPaired() &&
       (device->IsConnected() || !device->IsConnectable()))) {
    return;
  }
  if (!device->IsPaired() && device->IsPairable()) {
    // Show pairing dialog for the unpaired device.
    BluetoothPairingDialog::ShowDialog(device->GetAddress());
  } else {
    // Attempt to connect to the device.
    device->Connect(nullptr, base::DoNothing());
  }
}

void DeviceEmulatorMessageHandler::TouchpadExists(bool exists) {
  if (!IsJavascriptAllowed())
    return;
  FireWebUIListener("touchpad-exists-changed", base::Value(exists));
}

void DeviceEmulatorMessageHandler::HapticTouchpadExists(bool exists) {}

void DeviceEmulatorMessageHandler::MouseExists(bool exists) {
  if (!IsJavascriptAllowed())
    return;
  FireWebUIListener("mouse-exists-changed", base::Value(exists));
}

void DeviceEmulatorMessageHandler::PointingStickExists(bool exists) {
  // TODO(crbug.com/1114828): support fake pointing sticks.
}

void DeviceEmulatorMessageHandler::UpdateAudioNodes() {
  // Get every active audio node and create a dictionary to
  // send it to JavaScript.
  base::Value audio_nodes(base::Value::Type::LIST);
  for (const AudioNode& node :
       chromeos::FakeCrasAudioClient::Get()->node_list()) {
    base::Value audio_node(base::Value::Type::DICTIONARY);

    audio_node.GetDict().Set("isInput", node.is_input);
    audio_node.GetDict().Set("id", base::NumberToString(node.id));
    audio_node.GetDict().Set("deviceName", node.device_name);
    audio_node.GetDict().Set("type", node.type);
    audio_node.GetDict().Set("name", node.name);
    audio_node.GetDict().Set("active", node.active);

    audio_nodes.Append(std::move(audio_node));
  }

  FireWebUIListener(kAudioNodesUpdated, audio_nodes);
}

}  // namespace chromeos
