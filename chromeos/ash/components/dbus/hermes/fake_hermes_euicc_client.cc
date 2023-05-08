// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/fake_hermes_euicc_client.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char* kDefaultMccMnc = "310999";
const char* kFakeActivationCodePrefix = "1$SMDP.GSMA.COM$00000-00000-00000-000";
const char* kFakeProfilePathPrefix = "/org/chromium/Hermes/Profile/";
const char* kFakeIccidPrefix = "10000000000000000";
const char* kFakeProfileNamePrefix = "FakeCellularNetwork_";
const char* kFakeProfileNicknamePrefix = "FakeCellularNetworkNickname_";
const char* kFakeServiceProvider = "Fake Wireless";
const char* kFakeNetworkServicePathPrefix = "/service/cellular1";

dbus::Property<std::vector<dbus::ObjectPath>>& GetPendingProfiles(
    HermesEuiccClient::Properties* properties) {
  return features::IsSmdsDbusMigrationEnabled()
             ? properties->profiles()
             : properties->pending_carrier_profiles();
}

dbus::Property<std::vector<dbus::ObjectPath>>& GetInstalledProfiles(
    HermesEuiccClient::Properties* properties) {
  return features::IsSmdsDbusMigrationEnabled()
             ? properties->profiles()
             : properties->installed_carrier_profiles();
}

bool PopPendingProfile(HermesEuiccClient::Properties* properties,
                       dbus::ObjectPath carrier_profile_path) {
  std::vector<dbus::ObjectPath> profiles =
      GetPendingProfiles(properties).value();
  auto it = base::ranges::find(profiles, carrier_profile_path);
  if (it == profiles.end()) {
    return false;
  }

  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(*it);
  if (profile_properties->state().value() != hermes::profile::State::kPending) {
    return false;
  }

  profiles.erase(it);
  GetPendingProfiles(properties).ReplaceValue(profiles);
  return true;
}

dbus::ObjectPath PopPendingProfileWithActivationCode(
    HermesEuiccClient::Properties* euicc_properties,
    const std::string& activation_code) {
  std::vector<dbus::ObjectPath> profiles =
      GetPendingProfiles(euicc_properties).value();
  for (auto it = profiles.begin(); it != profiles.end(); it++) {
    dbus::ObjectPath carrier_profile_path = *it;
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(carrier_profile_path);
    if (profile_properties->activation_code().value() == activation_code) {
      profiles.erase(it);
      GetPendingProfiles(euicc_properties).ReplaceValue(profiles);
      return carrier_profile_path;
    }
  }
  return dbus::ObjectPath();
}

}  // namespace

FakeHermesEuiccClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : HermesEuiccClient::Properties(nullptr, callback) {}

FakeHermesEuiccClient::Properties::~Properties() = default;

void FakeHermesEuiccClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  DVLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeHermesEuiccClient::Properties::GetAll() {
  DVLOG(1) << "GetAll";
}

void FakeHermesEuiccClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  DVLOG(1) << "Set " << property->name();
  std::move(callback).Run(false);
}

FakeHermesEuiccClient::FakeHermesEuiccClient() = default;
FakeHermesEuiccClient::~FakeHermesEuiccClient() = default;

void FakeHermesEuiccClient::ClearEuicc(const dbus::ObjectPath& euicc_path) {
  PropertiesMap::iterator it = properties_map_.find(euicc_path);
  if (it == properties_map_.end())
    return;
  auto* profile_test = HermesProfileClient::Get()->GetTestInterface();
  HermesEuiccClient::Properties* properties = it->second.get();
  if (features::IsSmdsDbusMigrationEnabled()) {
    for (const auto& path : properties->profiles().value()) {
      profile_test->ClearProfile(path);
    }
  } else {
    for (const auto& path : properties->installed_carrier_profiles().value()) {
      profile_test->ClearProfile(path);
    }
    for (const auto& path : properties->pending_carrier_profiles().value()) {
      profile_test->ClearProfile(path);
    }
  }
  properties_map_.erase(it);
}

void FakeHermesEuiccClient::ResetPendingEventsRequested() {
  pending_event_requested_ = false;
}

dbus::ObjectPath FakeHermesEuiccClient::AddFakeCarrierProfile(
    const dbus::ObjectPath& euicc_path,
    hermes::profile::State state,
    const std::string& activation_code,
    AddCarrierProfileBehavior add_carrier_profile_behavior) {
  int index = fake_profile_counter_++;
  dbus::ObjectPath carrier_profile_path(
      base::StringPrintf("%s%02d", kFakeProfilePathPrefix, index));

  AddCarrierProfile(
      carrier_profile_path, euicc_path,
      base::StringPrintf("%s%02d", kFakeIccidPrefix, index),
      base::StringPrintf("%s%02d", kFakeProfileNicknamePrefix, index),
      base::StringPrintf("%s%02d", kFakeProfileNamePrefix, index),
      kFakeServiceProvider,
      activation_code.empty()
          ? base::StringPrintf("%s%02d", kFakeActivationCodePrefix, index)
          : activation_code,
      base::StringPrintf("%s%02d", kFakeNetworkServicePathPrefix, index), state,
      hermes::profile::ProfileClass::kOperational,
      add_carrier_profile_behavior);
  return carrier_profile_path;
}

void FakeHermesEuiccClient::AddCarrierProfile(
    const dbus::ObjectPath& path,
    const dbus::ObjectPath& euicc_path,
    const std::string& iccid,
    const std::string& name,
    const std::string& nickname,
    const std::string& service_provider,
    const std::string& activation_code,
    const std::string& network_service_path,
    hermes::profile::State state,
    hermes::profile::ProfileClass profile_class,
    AddCarrierProfileBehavior add_carrier_profile_behavior) {
  DVLOG(1) << "Adding new profile path=" << path.value() << ", name=" << name
           << ", state=" << state;
  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(path);
  profile_properties->iccid().ReplaceValue(iccid);
  profile_properties->service_provider().ReplaceValue(service_provider);
  profile_properties->mcc_mnc().ReplaceValue(kDefaultMccMnc);
  profile_properties->activation_code().ReplaceValue(activation_code);
  profile_properties->name().ReplaceValue(name);
  profile_properties->nick_name().ReplaceValue(nickname);
  profile_properties->state().ReplaceValue(state);
  profile_properties->profile_class().ReplaceValue(profile_class);
  profile_service_path_map_[path] = network_service_path;

  Properties* euicc_properties = GetProperties(euicc_path);
  if (state == hermes::profile::State::kPending) {
    std::vector<dbus::ObjectPath> profiles =
        GetPendingProfiles(euicc_properties).value();
    profiles.push_back(path);
    GetPendingProfiles(euicc_properties).ReplaceValue(profiles);
    return;
  }

  bool should_create_service =
      add_carrier_profile_behavior ==
          AddCarrierProfileBehavior::kAddProfileWithService ||
      add_carrier_profile_behavior ==
          AddCarrierProfileBehavior::kAddDelayedProfileWithService;
  if (should_create_service)
    CreateCellularService(euicc_path, path);

  bool should_delay_install =
      add_carrier_profile_behavior ==
          AddCarrierProfileBehavior::kAddDelayedProfileWithService ||
      add_carrier_profile_behavior ==
          AddCarrierProfileBehavior::kAddDelayedProfileWithoutService;
  if (should_delay_install) {
    QueueInstalledProfile(euicc_path, path);
    return;
  }

  std::vector<dbus::ObjectPath> profiles =
      GetInstalledProfiles(euicc_properties).value();
  profiles.push_back(path);
  GetInstalledProfiles(euicc_properties).ReplaceValue(profiles);
}

bool FakeHermesEuiccClient::RemoveCarrierProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path) {
  // Remove entry from profile service path map.
  auto profile_service_path_map_iter =
      profile_service_path_map_.find(carrier_profile_path);
  if (profile_service_path_map_iter == profile_service_path_map_.end()) {
    return false;
  }
  profile_service_path_map_.erase(profile_service_path_map_iter);

  // Remove profile from Euicc properties.
  Properties* euicc_properties = GetProperties(euicc_path);
  std::vector<dbus::ObjectPath> profiles =
      GetInstalledProfiles(euicc_properties).value();
  auto profiles_iter = base::ranges::find(profiles, carrier_profile_path);
  if (profiles_iter == profiles.end()) {
    return false;
  }

  profiles.erase(profiles_iter);
  GetInstalledProfiles(euicc_properties).ReplaceValue(profiles);

  // Remove profile dbus object.
  HermesProfileClient::Get()->GetTestInterface()->ClearProfile(
      carrier_profile_path);
  return true;
}

void FakeHermesEuiccClient::QueueHermesErrorStatus(
    HermesResponseStatus status) {
  error_status_queue_.push(status);
}

void FakeHermesEuiccClient::SetNextInstallProfileFromActivationCodeResult(
    HermesResponseStatus status) {
  CHECK(status != HermesResponseStatus::kSuccess);
  next_install_profile_result_ = status;
}

void FakeHermesEuiccClient::SetInteractiveDelay(base::TimeDelta delay) {
  interactive_delay_ = delay;
}

std::string FakeHermesEuiccClient::GenerateFakeActivationCode() {
  return base::StringPrintf("%s-%04d", kFakeActivationCodePrefix,
                            fake_profile_counter_++);
}

bool FakeHermesEuiccClient::GetLastRefreshProfilesRestoreSlotArg() {
  return last_restore_slot_arg_;
}

void FakeHermesEuiccClient::InstallProfileFromActivationCode(
    const dbus::ObjectPath& euicc_path,
    const std::string& activation_code,
    const std::string& confirmation_code,
    InstallCarrierProfileCallback callback) {
  if (next_install_profile_result_.has_value()) {
    std::move(callback).Run(next_install_profile_result_.value(),
                            /*carrier_profile_path=*/nullptr);
    next_install_profile_result_ = absl::nullopt;
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoInstallProfileFromActivationCode,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     activation_code, confirmation_code, std::move(callback)),
      interactive_delay_);
}

void FakeHermesEuiccClient::InstallPendingProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& confirmation_code,
    HermesResponseCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoInstallPendingProfile,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     carrier_profile_path, confirmation_code,
                     std::move(callback)),
      interactive_delay_);
}

void FakeHermesEuiccClient::RefreshInstalledProfiles(
    const dbus::ObjectPath& euicc_path,
    bool restore_slot,
    HermesResponseCallback callback) {
  last_restore_slot_arg_ = restore_slot;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoRequestInstalledProfiles,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     std::move(callback)),
      interactive_delay_);
}

void FakeHermesEuiccClient::RefreshSmdxProfiles(
    const dbus::ObjectPath& euicc_path,
    const std::string& activation_code,
    bool restore_slot,
    RefreshSmdxProfilesCallback callback) {
  DCHECK(ash::features::IsSmdsDbusMigrationEnabled());
  last_restore_slot_arg_ = restore_slot;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoRefreshSmdxProfiles,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     activation_code, std::move(callback)),
      interactive_delay_);
}

void FakeHermesEuiccClient::RequestPendingProfiles(
    const dbus::ObjectPath& euicc_path,
    const std::string& root_smds,
    HermesResponseCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoRequestPendingProfiles,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     std::move(callback)),
      interactive_delay_);
}

void FakeHermesEuiccClient::UninstallProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path,
    HermesResponseCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoUninstallProfile,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path,
                     carrier_profile_path, std::move(callback)),
      interactive_delay_);
}

void FakeHermesEuiccClient::ResetMemory(
    const dbus::ObjectPath& euicc_path,
    hermes::euicc::ResetOptions reset_option,
    HermesResponseCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::DoResetMemory,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path, reset_option,
                     std::move(callback)),
      interactive_delay_);
}

FakeHermesEuiccClient::Properties* FakeHermesEuiccClient::GetProperties(
    const dbus::ObjectPath& euicc_path) {
  auto it = properties_map_.find(euicc_path);
  if (it != properties_map_.end()) {
    return it->second.get();
  }

  DVLOG(1) << "Creating new Fake Euicc object path =" << euicc_path.value();
  std::unique_ptr<Properties> properties(new Properties(
      base::BindRepeating(&FakeHermesEuiccClient::CallNotifyPropertyChanged,
                          weak_ptr_factory_.GetWeakPtr(), euicc_path)));
  properties_map_[euicc_path] = std::move(properties);
  return properties_map_[euicc_path].get();
}

HermesEuiccClient::TestInterface* FakeHermesEuiccClient::GetTestInterface() {
  return this;
}

void FakeHermesEuiccClient::DoInstallProfileFromActivationCode(
    const dbus::ObjectPath& euicc_path,
    const std::string& activation_code,
    const std::string& confirmation_code,
    InstallCarrierProfileCallback callback) {
  DVLOG(1) << "Installing profile from activation code: code="
           << activation_code << ", confirmation_code=" << confirmation_code;
  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front(), nullptr);
    error_status_queue_.pop();
    return;
  }

  if (!base::StartsWith(activation_code, kFakeActivationCodePrefix,
                        base::CompareCase::SENSITIVE)) {
    std::move(callback).Run(HermesResponseStatus::kErrorInvalidActivationCode,
                            nullptr);
    return;
  }

  Properties* euicc_properties = GetProperties(euicc_path);

  dbus::ObjectPath profile_path =
      PopPendingProfileWithActivationCode(euicc_properties, activation_code);
  if (profile_path.IsValid()) {
    // Move pending profile to installed.
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path);
    profile_properties->state().ReplaceValue(hermes::profile::State::kInactive);

    std::vector<dbus::ObjectPath> profiles =
        GetInstalledProfiles(euicc_properties).value();
    profiles.push_back(profile_path);
    GetInstalledProfiles(euicc_properties).ReplaceValue(profiles);
  } else {
    // Create a new installed profile with given activation code.
    profile_path = AddFakeCarrierProfile(
        euicc_path, hermes::profile::State::kInactive, activation_code,
        AddCarrierProfileBehavior::kAddProfileWithService);
  }
  CreateCellularService(euicc_path, profile_path);

  std::move(callback).Run(HermesResponseStatus::kSuccess, &profile_path);
}

void FakeHermesEuiccClient::DoInstallPendingProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& confirmation_code,
    HermesResponseCallback callback) {
  DVLOG(1) << "Installing pending profile: path="
           << carrier_profile_path.value()
           << ", confirmation_code=" << confirmation_code;
  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front());
    error_status_queue_.pop();
    return;
  }

  Properties* euicc_properties = GetProperties(euicc_path);
  if (!PopPendingProfile(euicc_properties, carrier_profile_path)) {
    std::move(callback).Run(HermesResponseStatus::kErrorUnknown);
    return;
  }

  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(carrier_profile_path);
  profile_properties->state().ReplaceValue(hermes::profile::State::kInactive);

  std::vector<dbus::ObjectPath> profiles =
      GetInstalledProfiles(euicc_properties).value();
  profiles.push_back(carrier_profile_path);
  GetInstalledProfiles(euicc_properties).ReplaceValue(profiles);

  CreateCellularService(euicc_path, carrier_profile_path);

  std::move(callback).Run(HermesResponseStatus::kSuccess);
}

void FakeHermesEuiccClient::DoRequestInstalledProfiles(
    const dbus::ObjectPath& euicc_path,
    HermesResponseCallback callback) {
  DVLOG(1) << "Installed Profiles Requested";
  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front());
    error_status_queue_.pop();
    return;
  }

  auto iter = installed_profile_queue_map_.find(euicc_path);
  if (iter != installed_profile_queue_map_.end() && !iter->second->empty()) {
    InstalledProfileQueue* installed_profile_queue = iter->second.get();
    Properties* euicc_properties = GetProperties(euicc_path);
    std::vector<dbus::ObjectPath> profiles =
        GetInstalledProfiles(euicc_properties).value();
    while (!installed_profile_queue->empty()) {
      profiles.push_back(installed_profile_queue->front());
      installed_profile_queue->pop();
    }
    GetInstalledProfiles(euicc_properties).ReplaceValue(profiles);
  }
  std::move(callback).Run(HermesResponseStatus::kSuccess);
}

void FakeHermesEuiccClient::DoRefreshSmdxProfiles(
    const dbus::ObjectPath& euicc_path,
    const std::string& activation_code,
    RefreshSmdxProfilesCallback callback) {
  // Use CHECK() here since the only caller has a DCHECK().
  CHECK(ash::features::IsSmdsDbusMigrationEnabled());

  DVLOG(1) << "Refresh SM-DX Profiles Requested";

  std::vector<dbus::ObjectPath> profile_paths;

  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front(), profile_paths);
    error_status_queue_.pop();
    return;
  }

  // Configure a single profile that uses the activation code that was provided
  // and immediately return the path to this profile.
  profile_paths.push_back(AddFakeCarrierProfile(
      euicc_path, hermes::profile::State::kPending, activation_code,
      AddCarrierProfileBehavior::kAddProfileWithService));

  std::move(callback).Run(HermesResponseStatus::kSuccess, profile_paths);
}

void FakeHermesEuiccClient::DoRequestPendingProfiles(
    const dbus::ObjectPath& euicc_path,
    HermesResponseCallback callback) {
  DVLOG(1) << "Pending Profiles Requested";
  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front());
    error_status_queue_.pop();
    return;
  }

  if (!pending_event_requested_) {
    AddFakeCarrierProfile(euicc_path, hermes::profile::State::kPending, "",
                          AddCarrierProfileBehavior::kAddProfileWithService);
    pending_event_requested_ = true;
  }
  std::move(callback).Run(HermesResponseStatus::kSuccess);
}

void FakeHermesEuiccClient::DoUninstallProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path,
    HermesResponseCallback callback) {
  if (!error_status_queue_.empty()) {
    std::move(callback).Run(error_status_queue_.front());
    error_status_queue_.pop();
    return;
  }

  // TODO(azeemarshad): Remove Shill service after removing carrier profile.
  bool remove_success = RemoveCarrierProfile(euicc_path, carrier_profile_path);
  std::move(callback).Run(remove_success
                              ? HermesResponseStatus::kSuccess
                              : HermesResponseStatus::kErrorInvalidIccid);
}

void FakeHermesEuiccClient::DoResetMemory(
    const dbus::ObjectPath& euicc_path,
    hermes::euicc::ResetOptions reset_option,
    HermesResponseCallback callback) {
  HermesEuiccClient::Properties* properties = GetProperties(euicc_path);
  while (true) {
    const dbus::Property<std::vector<dbus::ObjectPath>>& profiles =
        GetInstalledProfiles(properties);
    if (profiles.value().empty()) {
      break;
    }

    // Use a copy of profile_path since it will be deallocated along with the
    // profile.
    const dbus::ObjectPath profile_path = profiles.value().front();
    bool remove_success = RemoveCarrierProfile(euicc_path, profile_path);
    CHECK(remove_success);
  }
  std::move(callback).Run(HermesResponseStatus::kSuccess);
}

// Creates cellular service in shill for the given carrier profile path.
// This simulates the expected hermes - shill interaction when a new carrier
// profile is installed on the device through Hermes. Shill will be notified and
// it then creates cellular services with matching ICCID for this profile.
void FakeHermesEuiccClient::CreateCellularService(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& carrier_profile_path) {
  const std::string& service_path =
      profile_service_path_map_[carrier_profile_path];
  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(carrier_profile_path);
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path);
  ShillServiceClient::TestInterface* service_test =
      ShillServiceClient::Get()->GetTestInterface();
  service_test->AddService(service_path,
                           "esim_guid" + properties->iccid().value(),
                           properties->name().value(), shill::kTypeCellular,
                           shill::kStateIdle, true);
  service_test->SetServiceProperty(
      service_path, shill::kEidProperty,
      base::Value(euicc_properties->eid().value()));
  service_test->SetServiceProperty(service_path, shill::kIccidProperty,
                                   base::Value(properties->iccid().value()));
  service_test->SetServiceProperty(
      service_path, shill::kImsiProperty,
      base::Value(properties->iccid().value() + "-IMSI"));
  service_test->SetServiceProperty(
      service_path, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_test->SetServiceProperty(service_path, shill::kConnectableProperty,
                                   base::Value(false));
  service_test->SetServiceProperty(service_path, shill::kVisibleProperty,
                                   base::Value(true));

  ShillProfileClient::TestInterface* profile_test =
      ShillProfileClient::Get()->GetTestInterface();
  profile_test->AddService(ShillProfileClient::GetSharedProfilePath(),
                           service_path);
}

void FakeHermesEuiccClient::CallNotifyPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeHermesEuiccClient::NotifyPropertyChanged,
                     base::Unretained(this), object_path, property_name));
}

void FakeHermesEuiccClient::NotifyPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DVLOG(1) << "Property changed path=" << object_path.value()
           << ", property=" << property_name;
  for (auto& observer : observers()) {
    observer.OnEuiccPropertyChanged(object_path, property_name);
  }
}

void FakeHermesEuiccClient::QueueInstalledProfile(
    const dbus::ObjectPath& euicc_path,
    const dbus::ObjectPath& profile_path) {
  auto iter = installed_profile_queue_map_.find(euicc_path);
  if (iter != installed_profile_queue_map_.end()) {
    iter->second->push(profile_path);
    return;
  }

  std::unique_ptr<InstalledProfileQueue> installed_profile_queue =
      std::make_unique<InstalledProfileQueue>();
  installed_profile_queue->push(profile_path);
  installed_profile_queue_map_[euicc_path] = std::move(installed_profile_queue);
}

}  // namespace ash
