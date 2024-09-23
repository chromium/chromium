// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/fake_hermes_profile_client.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/hermes/fake_hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char* kCellularDevicePath = "/device/cellular1";

const char* kDefaultCountry = "US";
const char* kDefaultOperatorName = "Network Operator";
const char* kDefaultOperatorCode = "123456";
const char* kDefaultOperatorCountry = "US";
const char* kDefaultOperatorUuid = "12345678-1234-1234-1234-123456789012";

}  // namespace

FakeHermesProfileClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : HermesProfileClient::Properties(nullptr, callback) {}

FakeHermesProfileClient::Properties::~Properties() = default;

void FakeHermesProfileClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  DVLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeHermesProfileClient::Properties::GetAll() {
  DVLOG(1) << "GetAll";
}

void FakeHermesProfileClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  DVLOG(1) << "Set " << property->name();
  if (property->name() == nick_name().name()) {
    // Only nickname property is read/write.
    std::move(callback).Run(true);
    property->ReplaceValueWithSetValue();
  } else {
    std::move(callback).Run(false);
  }
}

FakeHermesProfileClient::FakeHermesProfileClient() = default;
FakeHermesProfileClient::~FakeHermesProfileClient() = default;

void FakeHermesProfileClient::ClearProfile(
    const dbus::ObjectPath& carrier_profile_path) {
  properties_map_.erase(carrier_profile_path);
}

void FakeHermesProfileClient::SetEnableProfileBehavior(
    EnableProfileBehavior enable_profile_behavior) {
  enable_profile_behavior_ = enable_profile_behavior;
}

void FakeHermesProfileClient::SetNextEnableCarrierProfileResult(
    HermesResponseStatus status) {
  CHECK(status != HermesResponseStatus::kSuccess);
  next_enable_carrier_profile_result_ = status;
}

void FakeHermesProfileClient::EnableCarrierProfile(
    const dbus::ObjectPath& object_path,
    HermesResponseCallback callback) {
  DVLOG(1) << "Enabling Carrier Profile path=" << object_path.value();

  if (next_enable_carrier_profile_result_.has_value()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  next_enable_carrier_profile_result_.value()));
    next_enable_carrier_profile_result_ = std::nullopt;
    return;
  }

  // Update carrier profile states.
  HermesProfileClient::Properties* properties = GetProperties(object_path);
  if (!properties) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  HermesResponseStatus::kErrorUnknown));
    return;
  }
  if (properties->state().value() == hermes::profile::State::kActive) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  HermesResponseStatus::kErrorAlreadyEnabled));
    return;
  }
  for (auto& item : properties_map_) {
    auto& state = item.second.get()->state();
    if (state.value() != hermes::profile::State::kActive) {
      continue;
    }
    state.ReplaceValue(hermes::profile::State::kInactive);
  }
  properties->state().ReplaceValue(hermes::profile::State::kActive);

  // Update cellular device properties to match this profile.
  UpdateCellularDevice(properties);

  // Update all cellular services to set their connection state and connectable
  // properties. The newly enabled profile will have connectable set to true
  // if |enable_profile_behavior_| indicates that it should be.
  bool connectable =
      enable_profile_behavior_ != EnableProfileBehavior::kNotConnectable;
  UpdateCellularServices(properties->iccid().value(), connectable);

  // Make sure the SIM slot info is updated to reflect this change.
  HermesEuiccClient::Get()->GetTestInterface()->UpdateShillDeviceSimSlotInfo();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), HermesResponseStatus::kSuccess));
}

void FakeHermesProfileClient::DisableCarrierProfile(
    const dbus::ObjectPath& object_path,
    HermesResponseCallback callback) {
  DVLOG(1) << "Disabling Carrier Profile path=" << object_path.value();
  HermesProfileClient::Properties* properties = GetProperties(object_path);
  if (!properties) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  HermesResponseStatus::kErrorUnknown));
    return;
  }
  if (properties->state().value() == hermes::profile::State::kInactive) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  HermesResponseStatus::kErrorAlreadyDisabled));
    return;
  }
  properties->state().ReplaceValue(hermes::profile::State::kInactive);

  // The newly disabled profile should have connectable set to false.
  UpdateCellularServices(properties->iccid().value(), /*connectable=*/false);

  // Make sure the SIM slot info is updated to reflect this change.
  HermesEuiccClient::Get()->GetTestInterface()->UpdateShillDeviceSimSlotInfo();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), HermesResponseStatus::kSuccess));
}

void FakeHermesProfileClient::RenameProfile(const dbus::ObjectPath& object_path,
                                            const std::string& new_name,
                                            HermesResponseCallback callback) {
  DVLOG(1) << "Renaming carrier profile name to " << new_name
           << " on path: " << object_path.value();
  HermesProfileClient::Properties* properties = GetProperties(object_path);
  if (!properties) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  HermesResponseStatus::kErrorUnknown));
    return;
  }
  properties->nick_name().ReplaceValue(new_name);
  NotifyPropertyChanged(object_path, hermes::profile::kNicknameProperty);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), HermesResponseStatus::kSuccess));
}

HermesProfileClient::Properties* FakeHermesProfileClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  auto it = properties_map_.find(object_path);
  if (it != properties_map_.end()) {
    return it->second.get();
  }

  DVLOG(1) << "Creating new Fake Profile object path =" << object_path.value();
  std::unique_ptr<Properties> properties(new Properties(
      base::BindRepeating(&FakeHermesProfileClient::CallNotifyPropertyChanged,
                          base::Unretained(this), object_path)));
  properties_map_[object_path] = std::move(properties);
  return properties_map_[object_path].get();
}

HermesProfileClient::TestInterface*
FakeHermesProfileClient::GetTestInterface() {
  return this;
}

// Updates the Shill Cellular device properties so that they match the given
// profile |properties|. This simulates expected hermes - shill interaction
// when a carrier profile is enabled. Shill updates the active cellular device
// properties so that it matches the currently enabled carrier profile.
void FakeHermesProfileClient::UpdateCellularDevice(
    HermesProfileClient::Properties* properties) {
  ShillDeviceClient::TestInterface* device_test =
      ShillDeviceClient::Get()->GetTestInterface();

  // Update the cellular device properties so that they match the carrier
  // profile that was just enabled.
  base::Value::Dict home_provider;
  home_provider.Set(shill::kNameProperty,
                    properties->service_provider().value());
  home_provider.Set(shill::kCountryProperty, kDefaultCountry);
  home_provider.Set(shill::kNetworkIdProperty, properties->mcc_mnc().value());
  device_test->SetDeviceProperty(kCellularDevicePath,
                                 shill::kHomeProviderProperty,
                                 base::Value(std::move(home_provider)), true);
}

void FakeHermesProfileClient::UpdateCellularServices(const std::string& iccid,
                                                     bool connectable) {
  ShillManagerClient::TestInterface* manager_test =
      ShillManagerClient::Get()->GetTestInterface();
  ShillServiceClient::TestInterface* service_test =
      ShillServiceClient::Get()->GetTestInterface();

  base::Value::List service_list = manager_test->GetEnabledServiceList();
  for (const base::Value& service_path : service_list) {
    const base::Value::Dict* properties =
        service_test->GetServiceProperties(service_path.GetString());
    const std::string* type = properties->FindString(shill::kTypeProperty);
    const std::string* service_iccid =
        properties->FindString(shill::kIccidProperty);
    if (!service_iccid || !type || *type != shill::kTypeCellular)
      continue;

    bool is_current_service = iccid == *service_iccid;
    // All services except one with given |iccid| will have connectable set to
    // false.
    bool service_connectable = is_current_service ? connectable : false;
    service_test->SetServiceProperty(service_path.GetString(),
                                     shill::kConnectableProperty,
                                     base::Value(service_connectable));
    const char* service_state =
        is_current_service &&
                enable_profile_behavior_ ==
                    EnableProfileBehavior::kConnectableAndConnected
            ? shill::kStateOnline
            : shill::kStateIdle;
    service_test->SetServiceProperty(service_path.GetString(),
                                     shill::kStateProperty,
                                     base::Value(service_state));

    if (!is_current_service) {
      ShillServiceClient::Get()->ClearProperty(
          dbus::ObjectPath(service_path.GetString()),
          shill::kServingOperatorProperty, base::DoNothing(),
          base::DoNothing());
      continue;
    }

    // Set the operator information for the active eSIM profile.
    service_test->SetServiceProperty(
        service_path.GetString(), shill::kServingOperatorProperty,
        base::Value(
            base::Value::Dict()
                .Set(shill::kOperatorNameKey, kDefaultOperatorName)
                .Set(shill::kOperatorCodeKey, kDefaultOperatorCode)
                .Set(shill::kOperatorCountryKey, kDefaultOperatorCountry)
                .Set(shill::kOperatorUuidKey, kDefaultOperatorUuid)));
  }
}

void FakeHermesProfileClient::CallNotifyPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesProfileClient::NotifyPropertyChanged,
                     base::Unretained(this), object_path, property_name));
}

void FakeHermesProfileClient::NotifyPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DVLOG(1) << "Property changed path=" << object_path.value()
           << ", property=" << property_name;
  for (auto& observer : observers()) {
    observer.OnCarrierProfilePropertyChanged(object_path, property_name);
  }
}

}  // namespace ash
