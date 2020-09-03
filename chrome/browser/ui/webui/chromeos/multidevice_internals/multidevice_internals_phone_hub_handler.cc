// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_internals/multidevice_internals_phone_hub_handler.h"

#include "ash/public/cpp/system_tray.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace chromeos {
namespace multidevice {

namespace {

// Fake Favicon colors used for coloring the fake favicon bitmaps.
enum class FaviconType {
  kPink = 0,
  kRed = 1,
  kGreen = 2,
  kBlue = 3,
  kYellow = 4,
};

const SkBitmap FaviconNumToBitmap(FaviconType favicon_num) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  switch (favicon_num) {
    case FaviconType::kPink:
      bitmap.eraseARGB(0, 255, 192, 203);
      break;
    case FaviconType::kRed:
      bitmap.eraseARGB(0, 255, 0, 0);
      break;
    case FaviconType::kGreen:
      bitmap.eraseARGB(0, 0, 255, 0);
      break;
    case FaviconType::kBlue:
      bitmap.eraseARGB(0, 0, 0, 255);
      break;
    case FaviconType::kYellow:
      bitmap.eraseARGB(0, 255, 255, 0);
      break;
    default:
      break;
  }
  return bitmap;
}

base::Optional<phonehub::BrowserTabsModel::BrowserTabMetadata>
DictToBrowserTabMetadataModel(
    const base::DictionaryValue* browser_tab_metadata) {
  std::string url;
  if (!browser_tab_metadata->GetString("url", &url) || url.empty()) {
    return base::nullopt;
  }

  base::string16 title;
  if (!browser_tab_metadata->GetString("title", &title) || title.empty()) {
    return base::nullopt;
  }

  // JavaScript time stamps don't fit in int.
  double last_accessed_timestamp;
  if (!browser_tab_metadata->GetDouble("lastAccessedTimeStamp",
                                       &last_accessed_timestamp)) {
    return base::nullopt;
  }

  int favicon_type_as_int;
  if (!browser_tab_metadata->GetInteger("favicon", &favicon_type_as_int)) {
    return base::nullopt;
  }

  auto favicon_type = static_cast<FaviconType>(favicon_type_as_int);
  gfx::Image favicon =
      gfx::Image::CreateFrom1xBitmap(FaviconNumToBitmap(favicon_type));
  return phonehub::BrowserTabsModel::BrowserTabMetadata(
      GURL(url), title, base::Time::FromJsTime(last_accessed_timestamp),
      favicon);
}

}  // namespace

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

  web_ui()->RegisterMessageCallback(
      "setBrowserTabs",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetBrowserTabs,
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

void MultidevicePhoneHubHandler::HandleSetBrowserTabs(
    const base::ListValue* args) {
  const base::DictionaryValue* browser_tab_status_dict = nullptr;
  CHECK(args->GetDictionary(0, &browser_tab_status_dict));
  bool is_tab_sync_enabled;
  CHECK(browser_tab_status_dict->GetBoolean("isTabSyncEnabled",
                                            &is_tab_sync_enabled));

  if (!is_tab_sync_enabled) {
    fake_phone_hub_manager_->mutable_phone_model()->SetBrowserTabsModel(
        phonehub::BrowserTabsModel(is_tab_sync_enabled));
    PA_LOG(VERBOSE) << "Tab sync off; cleared browser tab metadata";
    return;
  }

  base::Optional<phonehub::BrowserTabsModel::BrowserTabMetadata> metadata_one;
  const base::DictionaryValue* browser_tab_one_metadata = nullptr;
  if (browser_tab_status_dict->GetDictionary("browserTabOneMetadata",
                                             &browser_tab_one_metadata)) {
    metadata_one = DictToBrowserTabMetadataModel(browser_tab_one_metadata);
  }

  base::Optional<phonehub::BrowserTabsModel::BrowserTabMetadata> metadata_two;
  const base::DictionaryValue* browser_tab_two_metadata = nullptr;
  if (browser_tab_status_dict->GetDictionary("browserTabTwoMetadata",
                                             &browser_tab_two_metadata)) {
    metadata_two = DictToBrowserTabMetadataModel(browser_tab_two_metadata);
  }

  // TODO(hsuregan): Add metadata_three and metadata_four.
  std::vector<base::Optional<phonehub::BrowserTabsModel::BrowserTabMetadata>>
      metadatas{{metadata_one, metadata_two}};
  std::sort(metadatas.begin(), metadatas.end());

  fake_phone_hub_manager_->mutable_phone_model()->SetBrowserTabsModel(
      phonehub::BrowserTabsModel(is_tab_sync_enabled, metadatas[1],
                                 metadatas[0]));

  if (metadatas[1].has_value())
    PA_LOG(VERBOSE) << "Set most recent browser tab to" << *metadatas[1];

  if (metadatas[0].has_value())
    PA_LOG(VERBOSE) << "Set second most recent browser tab to" << *metadatas[0];
}

}  // namespace multidevice
}  // namespace chromeos
