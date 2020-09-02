// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_internals/multidevice_internals_phone_hub_handler.h"

#include "ash/public/cpp/system_tray.h"
#include "chrome/browser/chromeos/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"

namespace chromeos {
namespace multidevice {

MultidevicePhoneHubHandler::MultidevicePhoneHubHandler() = default;

MultidevicePhoneHubHandler::~MultidevicePhoneHubHandler() {
  if (fake_phone_hub_manager_)
    SetSystemPhoneHubManagerEnabled();
}

void MultidevicePhoneHubHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setFakePhoneHubManagerEnabled",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::HandleSetFakePhoneHubManagerEnabled,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setFeatureStatus",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetFeatureStatus,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setFakePhoneStatus",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetFakePhoneStatus,
                          base::Unretained(this)));
}

void MultidevicePhoneHubHandler::SetSystemPhoneHubManagerEnabled() {
  PA_LOG(VERBOSE) << "Setting real Phone Hub Manager";
  fake_phone_hub_manager_.reset();
  Profile* profile = Profile::FromWebUI(web_ui());
  chromeos::phonehub::PhoneHubManager* phone_hub_manager =
      chromeos::phonehub::PhoneHubManagerFactory::GetForProfile(profile);
  ash::SystemTray::Get()->SetPhoneHubManager(phone_hub_manager);
}

void MultidevicePhoneHubHandler::SetFakePhoneHubManagerEnabled() {
  PA_LOG(VERBOSE) << "Setting fake Phone Hub Manager";
  fake_phone_hub_manager_ = std::make_unique<phonehub::FakePhoneHubManager>();
  ash::SystemTray::Get()->SetPhoneHubManager(fake_phone_hub_manager_.get());
}

void MultidevicePhoneHubHandler::HandleSetFakePhoneHubManagerEnabled(
    const base::ListValue* args) {
  AllowJavascript();
  bool enabled = false;
  CHECK(args->GetBoolean(0, &enabled));
  if (enabled) {
    SetFakePhoneHubManagerEnabled();
    return;
  }
  SetSystemPhoneHubManagerEnabled();
}

void MultidevicePhoneHubHandler::HandleSetFeatureStatus(
    const base::ListValue* args) {
  int feature_as_int = 0;
  CHECK(args->GetInteger(0, &feature_as_int));

  auto feature = static_cast<phonehub::FeatureStatus>(feature_as_int);
  PA_LOG(VERBOSE) << "Setting feature status to " << feature;
  fake_phone_hub_manager_->fake_feature_status_provider()->SetStatus(feature);
}

void MultidevicePhoneHubHandler::HandleSetFakePhoneStatus(
    const base::ListValue* args) {
  const base::DictionaryValue* phones_status_dict = nullptr;
  CHECK(args->GetDictionary(0, &phones_status_dict));
  int mobile_status_as_int;
  CHECK(phones_status_dict->GetInteger("mobileStatus", &mobile_status_as_int));
  auto mobile_status = static_cast<phonehub::PhoneStatusModel::MobileStatus>(
      mobile_status_as_int);

  int signal_strength_as_int;
  CHECK(phones_status_dict->GetInteger("signalStrength",
                                       &signal_strength_as_int));
  auto signal_strength =
      static_cast<phonehub::PhoneStatusModel::SignalStrength>(
          signal_strength_as_int);

  base::string16 mobile_provider;
  CHECK(phones_status_dict->GetString("mobileProvider", &mobile_provider));

  int charging_state_as_int;
  CHECK(
      phones_status_dict->GetInteger("chargingState", &charging_state_as_int));
  auto charging_state = static_cast<phonehub::PhoneStatusModel::ChargingState>(
      charging_state_as_int);

  int battery_saver_state_as_int;
  CHECK(phones_status_dict->GetInteger("batterySaverState",
                                       &battery_saver_state_as_int));
  auto battery_saver_state =
      static_cast<phonehub::PhoneStatusModel::BatterySaverState>(
          battery_saver_state_as_int);

  int battery_percentage;
  CHECK(
      phones_status_dict->GetInteger("batteryPercentage", &battery_percentage));

  phonehub::PhoneStatusModel::MobileConnectionMetadata connection_metadata = {
      .signal_strength = signal_strength,
      .mobile_provider = mobile_provider,
  };
  auto phone_status = phonehub::PhoneStatusModel(
      mobile_status, connection_metadata, charging_state, battery_saver_state,
      battery_percentage);
  fake_phone_hub_manager_->mutable_phone_model()->SetPhoneStatusModel(
      phone_status);

  PA_LOG(VERBOSE) << "Set phone status to -"
                  << "\n  mobile status: " << mobile_status
                  << "\n  signal strength: " << signal_strength
                  << "\n  mobile provider: " << mobile_provider
                  << "\n  charging state: " << charging_state
                  << "\n  battery saver state: " << battery_saver_state
                  << "\n  battery percentage: " << battery_percentage;
}

}  // namespace multidevice
}  // namespace chromeos
