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

// Fake image types used for fields that require gfx::Image().
enum class ImageType {
  kPink = 1,
  kRed = 2,
  kGreen = 3,
  kBlue = 4,
  kYellow = 5,
};

const SkBitmap ImageTypeToBitmap(ImageType image_type_num) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  switch (image_type_num) {
    case ImageType::kPink:
      bitmap.eraseARGB(0, 255, 192, 203);
      break;
    case ImageType::kRed:
      bitmap.eraseARGB(0, 255, 0, 0);
      break;
    case ImageType::kGreen:
      bitmap.eraseARGB(0, 0, 255, 0);
      break;
    case ImageType::kBlue:
      bitmap.eraseARGB(0, 0, 0, 255);
      break;
    case ImageType::kYellow:
      bitmap.eraseARGB(0, 255, 255, 0);
      break;
    default:
      break;
  }
  return bitmap;
}

phonehub::Notification::AppMetadata DictToAppMetadata(
    const base::DictionaryValue* app_metadata_dict) {
  base::string16 visible_app_name;
  CHECK(app_metadata_dict->GetString("visibleAppName", &visible_app_name));

  std::string package_name;
  CHECK(app_metadata_dict->GetString("packageName", &package_name));

  int icon_image_type_as_int;
  CHECK(app_metadata_dict->GetInteger("icon", &icon_image_type_as_int));

  auto icon_image_type = static_cast<ImageType>(icon_image_type_as_int);
  gfx::Image icon =
      gfx::Image::CreateFrom1xBitmap(ImageTypeToBitmap(icon_image_type));
  return phonehub::Notification::AppMetadata(visible_app_name, package_name,
                                             icon);
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

  int favicon_image_type_as_int;
  if (!browser_tab_metadata->GetInteger("favicon",
                                        &favicon_image_type_as_int)) {
    return base::nullopt;
  }

  auto favicon_image_type = static_cast<ImageType>(favicon_image_type_as_int);
  gfx::Image favicon =
      gfx::Image::CreateFrom1xBitmap(ImageTypeToBitmap(favicon_image_type));
  return phonehub::BrowserTabsModel::BrowserTabMetadata(
      GURL(url), title, base::Time::FromJsTime(last_accessed_timestamp),
      favicon);
}

}  // namespace

MultidevicePhoneHubHandler::MultidevicePhoneHubHandler() = default;

MultidevicePhoneHubHandler::~MultidevicePhoneHubHandler() {
  if (fake_phone_hub_manager_)
    EnableRealPhoneHubManager();
}

void MultidevicePhoneHubHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setFakePhoneHubManagerEnabled",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::HandleEnableFakePhoneHubManager,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setFeatureStatus",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetFeatureStatus,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setFakePhoneName",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetFakePhoneName,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setFakePhoneStatus",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetFakePhoneStatus,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setBrowserTabs",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetBrowserTabs,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setNotification",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetNotification,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "removeNotification",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleRemoveNotification,
                          base::Unretained(this)));
}

void MultidevicePhoneHubHandler::EnableRealPhoneHubManager() {
  // If no FakePhoneHubManager is active, return early. This ensures that we
  // don't unnecessarily re-initialize the Phone Hub UI.
  if (!fake_phone_hub_manager_)
    return;

  PA_LOG(VERBOSE) << "Setting real Phone Hub Manager";
  fake_phone_hub_manager_.reset();
  Profile* profile = Profile::FromWebUI(web_ui());
  chromeos::phonehub::PhoneHubManager* phone_hub_manager =
      chromeos::phonehub::PhoneHubManagerFactory::GetForProfile(profile);
  ash::SystemTray::Get()->SetPhoneHubManager(phone_hub_manager);
}

void MultidevicePhoneHubHandler::EnableFakePhoneHubManager() {
  DCHECK(!fake_phone_hub_manager_);
  PA_LOG(VERBOSE) << "Setting fake Phone Hub Manager";
  fake_phone_hub_manager_ = std::make_unique<phonehub::FakePhoneHubManager>();
  ash::SystemTray::Get()->SetPhoneHubManager(fake_phone_hub_manager_.get());
}

void MultidevicePhoneHubHandler::HandleEnableFakePhoneHubManager(
    const base::ListValue* args) {
  AllowJavascript();
  bool enabled = false;
  CHECK(args->GetBoolean(0, &enabled));
  if (enabled) {
    EnableFakePhoneHubManager();
    return;
  }
  EnableRealPhoneHubManager();
}

void MultidevicePhoneHubHandler::HandleSetFeatureStatus(
    const base::ListValue* args) {
  int feature_as_int = 0;
  CHECK(args->GetInteger(0, &feature_as_int));

  auto feature = static_cast<phonehub::FeatureStatus>(feature_as_int);
  PA_LOG(VERBOSE) << "Setting feature status to " << feature;
  fake_phone_hub_manager_->fake_feature_status_provider()->SetStatus(feature);
}

void MultidevicePhoneHubHandler::HandleSetFakePhoneName(
    const base::ListValue* args) {
  base::string16 phone_name;
  CHECK(args->GetString(0, &phone_name));
  fake_phone_hub_manager_->mutable_phone_model()->SetPhoneName(phone_name);
  PA_LOG(VERBOSE) << "Set phone name to " << phone_name;
}

void MultidevicePhoneHubHandler::HandleSetFakePhoneStatus(
    const base::ListValue* args) {
  const base::DictionaryValue* phones_status_dict = nullptr;
  CHECK(args->GetDictionary(0, &phones_status_dict));

  base::string16 phone_name;
  CHECK(phones_status_dict->GetString("phoneName", &phone_name));

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

void MultidevicePhoneHubHandler::HandleSetNotification(
    const base::ListValue* args) {
  const base::DictionaryValue* notification_data_dict = nullptr;
  CHECK(args->GetDictionary(0, &notification_data_dict));

  int id;
  CHECK(notification_data_dict->GetInteger("id", &id));

  const base::DictionaryValue* app_metadata_dict = nullptr;
  CHECK(
      notification_data_dict->GetDictionary("appMetadata", &app_metadata_dict));
  phonehub::Notification::AppMetadata app_metadata =
      DictToAppMetadata(app_metadata_dict);

  // JavaScript time stamps don't fit in int.
  double js_timestamp;
  CHECK(notification_data_dict->GetDouble("timestamp", &js_timestamp));
  auto timestamp = base::Time::FromJsTime(js_timestamp);

  int importance_as_int;
  CHECK(notification_data_dict->GetInteger("importance", &importance_as_int));
  auto importance =
      static_cast<phonehub::Notification::Importance>(importance_as_int);

  int inline_reply_id;
  CHECK(notification_data_dict->GetInteger("inlineReplyId", &inline_reply_id));

  base::Optional<base::string16> opt_title;
  base::string16 title;
  if (notification_data_dict->GetString("title", &title) && !title.empty()) {
    opt_title = title;
  }

  base::Optional<base::string16> opt_text_content;
  base::string16 text_content;
  if (notification_data_dict->GetString("textContent", &text_content) &&
      !text_content.empty()) {
    opt_text_content = text_content;
  }

  base::Optional<gfx::Image> opt_shared_image;
  int shared_image_type_as_int;
  if (notification_data_dict->GetInteger("sharedImage",
                                         &shared_image_type_as_int) &&
      shared_image_type_as_int) {
    auto shared_image_type = static_cast<ImageType>(shared_image_type_as_int);
    opt_shared_image =
        gfx::Image::CreateFrom1xBitmap(ImageTypeToBitmap(shared_image_type));
  }

  base::Optional<gfx::Image> opt_contact_image;
  int contact_image_type_as_int;
  if (notification_data_dict->GetInteger("contactImage",
                                         &contact_image_type_as_int) &&
      contact_image_type_as_int) {
    auto shared_contact_image_type =
        static_cast<ImageType>(contact_image_type_as_int);
    opt_contact_image = gfx::Image::CreateFrom1xBitmap(
        ImageTypeToBitmap(shared_contact_image_type));
  }

  auto notification = phonehub::Notification(
      id, app_metadata, timestamp, importance, inline_reply_id, opt_title,
      opt_text_content, opt_shared_image, opt_contact_image);

  PA_LOG(VERBOSE) << "Set notification" << notification;
  fake_phone_hub_manager_->fake_notification_manager()->SetNotification(
      std::move(notification));
}

void MultidevicePhoneHubHandler::HandleRemoveNotification(
    const base::ListValue* args) {
  int notification_id = 0;
  CHECK(args->GetInteger(0, &notification_id));
  fake_phone_hub_manager_->fake_notification_manager()->RemoveNotification(
      notification_id);
  PA_LOG(VERBOSE) << "Removed notification with id " << notification_id;
}

}  // namespace multidevice
}  // namespace chromeos
