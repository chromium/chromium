// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/multidevice_internals/multidevice_internals_phone_hub_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/system_tray.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/camera_roll_item.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace ash::multidevice {

namespace {

const int kIconSize = 16;
const int kContactImageSize = 80;
const int kSharedImageSize = 400;
const int kCameraRollThumbnailSize = 96;

// Fake image types used for fields that require gfx::Image().
enum class ImageType {
  kPink = 1,
  kRed = 2,
  kGreen = 3,
  kBlue = 4,
  kYellow = 5,
};

const SkBitmap ImageTypeToBitmap(ImageType image_type_num, int size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  switch (image_type_num) {
    case ImageType::kPink:
      bitmap.eraseARGB(255, 255, 192, 203);
      break;
    case ImageType::kRed:
      bitmap.eraseARGB(255, 255, 0, 0);
      break;
    case ImageType::kGreen:
      bitmap.eraseARGB(255, 0, 255, 0);
      break;
    case ImageType::kBlue:
      bitmap.eraseARGB(255, 0, 0, 255);
      break;
    case ImageType::kYellow:
      bitmap.eraseARGB(255, 255, 255, 0);
      break;
    default:
      break;
  }
  return bitmap;
}

phonehub::Notification::AppMetadata DictToAppMetadata(
    const base::Value::Dict* app_metadata_dict) {
  const std::string* visible_app_name_ptr =
      app_metadata_dict->FindString("visibleAppName");
  CHECK(visible_app_name_ptr);
  std::u16string visible_app_name = base::UTF8ToUTF16(*visible_app_name_ptr);

  const std::string* package_name =
      app_metadata_dict->FindString("packageName");
  CHECK(package_name);

  std::optional<int> icon_image_type_as_int =
      app_metadata_dict->FindInt("icon");
  CHECK(icon_image_type_as_int);

  auto icon_image_type = static_cast<ImageType>(*icon_image_type_as_int);
  gfx::Image icon = gfx::Image::CreateFrom1xBitmap(
      ImageTypeToBitmap(icon_image_type, kIconSize));

  int user_id = app_metadata_dict->FindInt("userId").value_or(0);

  return phonehub::Notification::AppMetadata(
      visible_app_name, *package_name, /* color_icon= */ icon,
      /* monochrome_icon_mask= */ std::nullopt,
      /* icon_color= */ std::nullopt,
      /* icon_is_monochrome= */ false, user_id,
      phonehub::proto::AppStreamabilityStatus::STREAMABLE);
}

void TryAddingMetadata(
    const std::string& key,
    const base::Value::Dict* browser_tab_status_dict,
    std::vector<phonehub::BrowserTabsModel::BrowserTabMetadata>& metadatas) {
  const base::Value::Dict* browser_tab_metadata =
      browser_tab_status_dict->FindDict(key);

  if (!browser_tab_metadata)
    return;

  const std::string* url = browser_tab_metadata->FindString("url");
  if (!url || url->empty())
    return;

  const std::string* title = browser_tab_metadata->FindString("title");
  if (!title)
    return;

  // JavaScript time stamps don't fit in int.
  std::optional<double> last_accessed_timestamp =
      browser_tab_metadata->FindDouble("lastAccessedTimeStamp");
  if (!last_accessed_timestamp)
    return;

  int favicon_image_type_as_int =
      browser_tab_metadata->FindInt("favicon").value_or(0);
  if (!favicon_image_type_as_int)
    return;

  auto favicon_image_type = static_cast<ImageType>(favicon_image_type_as_int);
  gfx::Image favicon = gfx::Image::CreateFrom1xBitmap(
      ImageTypeToBitmap(favicon_image_type, kIconSize));

  auto metadata = phonehub::BrowserTabsModel::BrowserTabMetadata(
      GURL(*url), base::UTF8ToUTF16(*title),
      base::Time::FromMillisecondsSinceUnixEpoch(*last_accessed_timestamp),
      favicon);

  metadatas.push_back(metadata);
}

const SkBitmap RGB_Bitmap(U8CPU r, U8CPU g, U8CPU b, int size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseARGB(255, r, g, b);
  return bitmap;
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
      "setShowOnboardingFlow",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::HandleSetShowOnboardingFlow,
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

  web_ui()->RegisterMessageCallback(
      "enableDnd",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleEnableDnd,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setFindMyDeviceStatus",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::HandleSetFindMyDeviceStatus,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setTetherStatus",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetTetherStatus,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "resetShouldShowOnboardingUi",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::HandleResetShouldShowOnboardingUi,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "resetHasMultideviceFeatureSetupUiBeenDismissed",
      base::BindRepeating(
          &MultidevicePhoneHubHandler::
              HandleResetHasMultideviceFeatureSetupUiBeenDismissed,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setFakeCameraRoll",
      base::BindRepeating(&MultidevicePhoneHubHandler::HandleSetFakeCameraRoll,
                          base::Unretained(this)));
}

void MultidevicePhoneHubHandler::OnJavascriptDisallowed() {
  RemoveObservers();
}

void MultidevicePhoneHubHandler::AddObservers() {
  notification_manager_observation_.Observe(
      fake_phone_hub_manager_->fake_notification_manager());
  do_not_disturb_controller_observation_.Observe(
      fake_phone_hub_manager_->fake_do_not_disturb_controller());
  find_my_device_controller_observation_.Observe(
      fake_phone_hub_manager_->fake_find_my_device_controller());
  tether_controller_observation_.Observe(
      fake_phone_hub_manager_->fake_tether_controller());
  onboarding_ui_tracker_observation_.Observe(
      fake_phone_hub_manager_->fake_onboarding_ui_tracker());
  camera_roll_manager_observation_.Observe(
      fake_phone_hub_manager_->fake_camera_roll_manager());
}

void MultidevicePhoneHubHandler::RemoveObservers() {
  notification_manager_observation_.Reset();
  do_not_disturb_controller_observation_.Reset();
  find_my_device_controller_observation_.Reset();
  tether_controller_observation_.Reset();
  onboarding_ui_tracker_observation_.Reset();
  camera_roll_manager_observation_.Reset();
}

void MultidevicePhoneHubHandler::OnNotificationsRemoved(
    const base::flat_set<int64_t>& notification_ids) {
  base::Value::List removed_notification_id_js_list;
  for (const int64_t& id : notification_ids) {
    removed_notification_id_js_list.Append(static_cast<double>(id));
  }
  FireWebUIListener("removed-notification-ids",
                    removed_notification_id_js_list);
}

void MultidevicePhoneHubHandler::OnDndStateChanged() {
  bool is_dnd_enabled =
      fake_phone_hub_manager_->fake_do_not_disturb_controller()->IsDndEnabled();
  FireWebUIListener("is-dnd-enabled-changed", base::Value(is_dnd_enabled));
}

void MultidevicePhoneHubHandler::OnPhoneRingingStateChanged() {
  phonehub::FindMyDeviceController::Status ringing_status =
      fake_phone_hub_manager_->fake_find_my_device_controller()
          ->GetPhoneRingingStatus();

  FireWebUIListener("find-my-device-status-changed",
                    base::Value(static_cast<int>(ringing_status)));
}

void MultidevicePhoneHubHandler::OnTetherStatusChanged() {
  int status_as_int = static_cast<int>(
      fake_phone_hub_manager_->fake_tether_controller()->GetStatus());
  FireWebUIListener("tether-status-changed", base::Value(status_as_int));
}

void MultidevicePhoneHubHandler::OnShouldShowOnboardingUiChanged() {
  bool should_show_onboarding_ui =
      fake_phone_hub_manager_->fake_onboarding_ui_tracker()
          ->ShouldShowOnboardingUi();
  FireWebUIListener("should-show-onboarding-ui-changed",
                    base::Value(should_show_onboarding_ui));
}

void MultidevicePhoneHubHandler::OnCameraRollViewUiStateUpdated() {
  base::Value::Dict camera_roll_dict;
  camera_roll_dict.Set("isCameraRollEnabled",
                       fake_phone_hub_manager_->fake_camera_roll_manager()
                           ->is_camera_roll_enabled());
  FireWebUIListener("camera-roll-ui-view-state-updated", camera_roll_dict);
}

void MultidevicePhoneHubHandler::HandleEnableDnd(
    const base::Value::List& list) {
  CHECK(!list.empty());
  const bool enabled = list[0].GetBool();
  PA_LOG(VERBOSE) << "Setting Do Not Disturb state to " << enabled;
  fake_phone_hub_manager_->fake_do_not_disturb_controller()
      ->SetDoNotDisturbStateInternal(enabled,
                                     /*can_request_new_dnd_state=*/true);
}

void MultidevicePhoneHubHandler::HandleSetFindMyDeviceStatus(
    const base::Value::List& list) {
  CHECK_GE(list.size(), 1u);
  int status_as_int = list[0].GetInt();

  auto status =
      static_cast<phonehub::FindMyDeviceController::Status>(status_as_int);
  PA_LOG(VERBOSE) << "Setting phone ringing status to " << status;
  fake_phone_hub_manager_->fake_find_my_device_controller()
      ->SetPhoneRingingState(status);
}

void MultidevicePhoneHubHandler::HandleSetTetherStatus(
    const base::Value::List& list) {
  CHECK_GE(list.size(), 1u);
  int status_as_int = list[0].GetInt();

  auto status = static_cast<phonehub::TetherController::Status>(status_as_int);
  PA_LOG(VERBOSE) << "Setting tether status to " << status;
  fake_phone_hub_manager_->fake_tether_controller()->SetStatus(status);
}

void MultidevicePhoneHubHandler::EnableRealPhoneHubManager() {
  // If no FakePhoneHubManager is active, return early. This ensures that we
  // don't unnecessarily re-initialize the Phone Hub UI.
  if (!fake_phone_hub_manager_)
    return;

  PA_LOG(VERBOSE) << "Setting real Phone Hub Manager";
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* phone_hub_manager =
      phonehub::PhoneHubManagerFactory::GetForProfile(profile);
  SystemTray::Get()->SetPhoneHubManager(phone_hub_manager);

  RemoveObservers();
  fake_phone_hub_manager_.reset();
}

void MultidevicePhoneHubHandler::EnableFakePhoneHubManager() {
  // Don't create FakePhoneHubManager if it already exists to prevent UAF.
  if (fake_phone_hub_manager_)
    return;

  PA_LOG(VERBOSE) << "Setting fake Phone Hub Manager";
  fake_phone_hub_manager_ = std::make_unique<phonehub::FakePhoneHubManager>();
  SystemTray::Get()->SetPhoneHubManager(fake_phone_hub_manager_.get());
  AddObservers();
}

void MultidevicePhoneHubHandler::HandleEnableFakePhoneHubManager(
    const base::Value::List& list) {
  AllowJavascript();
  CHECK(!list.empty());
  const bool enabled = list[0].GetBool();
  if (enabled) {
    EnableFakePhoneHubManager();
    return;
  }
  EnableRealPhoneHubManager();
}

void MultidevicePhoneHubHandler::HandleSetFeatureStatus(
    const base::Value::List& list) {
  CHECK_GE(list.size(), 1u);
  int feature_as_int = list[0].GetInt();

  auto feature = static_cast<phonehub::FeatureStatus>(feature_as_int);
  PA_LOG(VERBOSE) << "Setting feature status to " << feature;
  fake_phone_hub_manager_->fake_feature_status_provider()->SetStatus(feature);
}

void MultidevicePhoneHubHandler::HandleSetShowOnboardingFlow(
    const base::Value::List& list) {
  CHECK(!list.empty());
  const bool show_onboarding_flow = list[0].GetBool();
  PA_LOG(VERBOSE) << "Setting show onboarding flow to " << show_onboarding_flow;
  fake_phone_hub_manager_->fake_onboarding_ui_tracker()
      ->SetShouldShowOnboardingUi(show_onboarding_flow);
}

void MultidevicePhoneHubHandler::HandleSetFakePhoneName(
    const base::Value::List& args_list) {
  CHECK_GE(args_list.size(), 1u);
  std::u16string phone_name = base::UTF8ToUTF16(args_list[0].GetString());
  fake_phone_hub_manager_->mutable_phone_model()->SetPhoneName(phone_name);
  PA_LOG(VERBOSE) << "Set phone name to " << phone_name;
}

void MultidevicePhoneHubHandler::HandleSetFakePhoneStatus(
    const base::Value::List& args) {
  CHECK(args[0].is_dict());
  const base::Value::Dict& phones_status_value = args[0].GetDict();

  std::optional<int> mobile_status_as_int =
      phones_status_value.FindInt("mobileStatus");
  CHECK(mobile_status_as_int);
  auto mobile_status = static_cast<phonehub::PhoneStatusModel::MobileStatus>(
      *mobile_status_as_int);

  std::optional<int> signal_strength_as_int =
      phones_status_value.FindInt("signalStrength");
  CHECK(signal_strength_as_int);

  auto signal_strength =
      static_cast<phonehub::PhoneStatusModel::SignalStrength>(
          *signal_strength_as_int);

  const std::string* mobile_provider_ptr =
      phones_status_value.FindString("mobileProvider");
  CHECK(mobile_provider_ptr);
  std::u16string mobile_provider = base::UTF8ToUTF16(*mobile_provider_ptr);

  std::optional<int> charging_state_as_int =
      phones_status_value.FindInt("chargingState");
  CHECK(charging_state_as_int);
  auto charging_state = static_cast<phonehub::PhoneStatusModel::ChargingState>(
      *charging_state_as_int);

  std::optional<int> battery_saver_state_as_int =
      phones_status_value.FindInt("batterySaverState");
  CHECK(battery_saver_state_as_int);
  auto battery_saver_state =
      static_cast<phonehub::PhoneStatusModel::BatterySaverState>(
          *battery_saver_state_as_int);

  std::optional<int> battery_percentage =
      phones_status_value.FindInt("batteryPercentage");
  CHECK(battery_percentage);

  phonehub::PhoneStatusModel::MobileConnectionMetadata connection_metadata = {
      .signal_strength = signal_strength,
      .mobile_provider = mobile_provider,
  };
  auto phone_status = phonehub::PhoneStatusModel(
      mobile_status, connection_metadata, charging_state, battery_saver_state,
      *battery_percentage);
  fake_phone_hub_manager_->mutable_phone_model()->SetPhoneStatusModel(
      phone_status);

  PA_LOG(VERBOSE) << "Set phone status to -"
                  << "\n  mobile status: " << mobile_status
                  << "\n  signal strength: " << signal_strength
                  << "\n  mobile provider: " << mobile_provider
                  << "\n  charging state: " << charging_state
                  << "\n  battery saver state: " << battery_saver_state
                  << "\n  battery percentage: " << *battery_percentage;
}

void MultidevicePhoneHubHandler::HandleSetBrowserTabs(
    const base::Value::List& args) {
  CHECK(args[0].is_dict());
  const base::Value::Dict& browser_tab_status_dict = args[0].GetDict();
  bool is_tab_sync_enabled =
      browser_tab_status_dict.FindBool("isTabSyncEnabled").value();

  if (!is_tab_sync_enabled) {
    fake_phone_hub_manager_->mutable_phone_model()->SetBrowserTabsModel(
        phonehub::BrowserTabsModel(is_tab_sync_enabled));
    PA_LOG(VERBOSE) << "Tab sync off; cleared browser tab metadata";
    return;
  }

  std::vector<phonehub::BrowserTabsModel::BrowserTabMetadata> metadatas;
  TryAddingMetadata("browserTabOneMetadata", &browser_tab_status_dict,
                    metadatas);
  TryAddingMetadata("browserTabTwoMetadata", &browser_tab_status_dict,
                    metadatas);

  fake_phone_hub_manager_->mutable_phone_model()->SetBrowserTabsModel(
      phonehub::BrowserTabsModel(is_tab_sync_enabled, metadatas));

  auto browser_tabs_model =
      fake_phone_hub_manager_->mutable_phone_model()->browser_tabs_model();
  CHECK(browser_tabs_model.has_value());

  // Log the most recently visited browser tab (at index 0) last.
  for (size_t i = metadatas.size(); i-- > 0;) {
    PA_LOG(VERBOSE) << "Set most recent browser tab number " << i
                    << " to: " << browser_tabs_model->most_recent_tabs()[i];
  }
}

void MultidevicePhoneHubHandler::HandleSetNotification(
    const base::Value::List& args) {
  CHECK(args[0].is_dict());
  const base::Value::Dict& notification_data_value = args[0].GetDict();

  std::optional<int> id = notification_data_value.FindInt("id");
  CHECK(id);

  const base::Value::Dict* app_metadata_dict =
      notification_data_value.FindDict("appMetadata");
  CHECK(app_metadata_dict);
  phonehub::Notification::AppMetadata app_metadata =
      DictToAppMetadata(app_metadata_dict);

  // JavaScript time stamps don't fit in int.
  std::optional<double> js_timestamp =
      notification_data_value.FindDouble("timestamp");
  CHECK(js_timestamp);
  auto timestamp = base::Time::FromMillisecondsSinceUnixEpoch(*js_timestamp);

  std::optional<int> importance_as_int =
      notification_data_value.FindInt("importance");
  CHECK(importance_as_int);
  auto importance =
      static_cast<phonehub::Notification::Importance>(*importance_as_int);

  std::optional<int> inline_reply_id =
      notification_data_value.FindInt("inlineReplyId");
  CHECK(inline_reply_id);

  std::optional<std::u16string> opt_title;
  const std::string* title = notification_data_value.FindString("title");
  if (title && !title->empty())
    opt_title = base::UTF8ToUTF16(*title);

  std::optional<std::u16string> opt_text_content;
  if (const std::string* text_content =
          notification_data_value.FindString("textContent")) {
    if (!text_content->empty())
      opt_text_content = base::UTF8ToUTF16(*text_content);
  }

  std::optional<gfx::Image> opt_shared_image;
  int shared_image_type_as_int =
      notification_data_value.FindInt("sharedImage").value_or(0);
  if (shared_image_type_as_int) {
    auto shared_image_type = static_cast<ImageType>(shared_image_type_as_int);
    opt_shared_image = gfx::Image::CreateFrom1xBitmap(
        ImageTypeToBitmap(shared_image_type, kSharedImageSize));
  }

  std::optional<gfx::Image> opt_contact_image;
  int contact_image_type_as_int =
      notification_data_value.FindInt("contactImage").value_or(0);
  if (contact_image_type_as_int) {
    auto shared_contact_image_type =
        static_cast<ImageType>(contact_image_type_as_int);
    opt_contact_image = gfx::Image::CreateFrom1xBitmap(
        ImageTypeToBitmap(shared_contact_image_type, kContactImageSize));
  }

  auto notification = phonehub::Notification(
      *id, app_metadata, timestamp, importance,
      phonehub::Notification::Category::kConversation,
      {{phonehub::Notification::ActionType::kInlineReply, *inline_reply_id}},
      phonehub::Notification::InteractionBehavior::kNone, opt_title,
      opt_text_content, opt_shared_image, opt_contact_image);

  PA_LOG(VERBOSE) << "Set notification" << notification;
  fake_phone_hub_manager_->fake_notification_manager()->SetNotification(
      std::move(notification));
}

void MultidevicePhoneHubHandler::HandleRemoveNotification(
    const base::Value::List& list) {
  CHECK_GE(list.size(), 1u);
  int notification_id = list[0].GetInt();
  fake_phone_hub_manager_->fake_notification_manager()->RemoveNotification(
      notification_id);
  PA_LOG(VERBOSE) << "Removed notification with id " << notification_id;
}

void MultidevicePhoneHubHandler::HandleResetShouldShowOnboardingUi(
    const base::Value::List& list) {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetBoolean(phonehub::prefs::kHideOnboardingUi, false);
  PA_LOG(VERBOSE) << "Reset kHideOnboardingUi pref";
}

void MultidevicePhoneHubHandler::
    HandleResetHasMultideviceFeatureSetupUiBeenDismissed(
        const base::Value::List& list) {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetBoolean(phonehub::prefs::kHasDismissedSetupRequiredUi, false);
  PA_LOG(VERBOSE) << "Reset kHasDismissedSetupRequiredUi pref";
}

void MultidevicePhoneHubHandler::HandleSetFakeCameraRoll(
    const base::Value::List& args) {
  const base::Value& camera_roll_dict = args[0];
  CHECK(camera_roll_dict.is_dict());

  std::optional<bool> is_camera_roll_enabled =
      camera_roll_dict.GetDict().FindBool("isCameraRollEnabled");
  CHECK(is_camera_roll_enabled);

  fake_phone_hub_manager_->fake_camera_roll_manager()
      ->SetIsCameraRollAvailableToBeEnabled(!*is_camera_roll_enabled);

  std::optional<bool> is_file_access_granted =
      camera_roll_dict.GetDict().FindBool("isFileAccessGranted");
  CHECK(is_file_access_granted);

  fake_phone_hub_manager_->fake_camera_roll_manager()
      ->SetIsAndroidStorageGranted(*is_file_access_granted);

  std::optional<int> number_of_thumbnails =
      camera_roll_dict.GetDict().FindInt("numberOfThumbnails");
  CHECK(number_of_thumbnails);

  std::optional<int> file_type_as_int =
      camera_roll_dict.GetDict().FindInt("fileType");
  CHECK(file_type_as_int);
  const char* file_type;
  const char* file_ext;
  if (*file_type_as_int == 0) {
    file_type = "image/jpeg";
    file_ext = ".jpg";
  } else {
    file_type = "video/mp4";
    file_ext = ".mp4";
  }

  if (*number_of_thumbnails == 0) {
    fake_phone_hub_manager_->fake_camera_roll_manager()->ClearCurrentItems();
  } else {
    std::vector<phonehub::CameraRollItem> items;
    // Create items in descending key order
    for (int i = *number_of_thumbnails; i > 0; --i) {
      phonehub::proto::CameraRollItemMetadata metadata;
      metadata.set_key(base::NumberToString(i));
      metadata.set_mime_type(file_type);
      metadata.set_last_modified_millis(1577865600 + i);
      metadata.set_file_size_bytes(123456);
      metadata.set_file_name("fake_file_" + base::NumberToString(i) + file_ext);

      gfx::Image thumbnail = gfx::Image::CreateFrom1xBitmap(RGB_Bitmap(
          255 - i * 192 / *number_of_thumbnails,
          63 + i * 192 / *number_of_thumbnails, 255, kCameraRollThumbnailSize));

      items.emplace_back(metadata, thumbnail);
    }
    fake_phone_hub_manager_->fake_camera_roll_manager()->SetCurrentItems(items);
  }

  std::optional<int> download_result_as_int =
      camera_roll_dict.GetDict().FindInt("downloadResult");
  CHECK(download_result_as_int);
  const char* download_result;
  if (*download_result_as_int == 0) {
    download_result = "Download Success";
    fake_phone_hub_manager_->fake_camera_roll_manager()
        ->SetSimulatedDownloadError(false);
  } else {
    phonehub::CameraRollManager::Observer::DownloadErrorType error_type;
    switch (*download_result_as_int) {
      case 1:
        download_result = "Generic Error";
        error_type = phonehub::CameraRollManager::Observer::DownloadErrorType::
            kGenericError;
        break;
      case 2:
        download_result = "Storage Error";
        error_type = phonehub::CameraRollManager::Observer::DownloadErrorType::
            kInsufficientStorage;
        break;
      case 3:
      default:
        download_result = "Network Error";
        error_type = phonehub::CameraRollManager::Observer::DownloadErrorType::
            kNetworkConnection;
        break;
    }
    fake_phone_hub_manager_->fake_camera_roll_manager()
        ->SetSimulatedDownloadError(true);
    fake_phone_hub_manager_->fake_camera_roll_manager()->SetSimulatedErrorType(
        error_type);
  }

  PA_LOG(VERBOSE) << "Setting fake Camera Roll to:\n  Feature enabled: "
                  << *is_camera_roll_enabled
                  << "\n  Access granted: " << *is_file_access_granted
                  << "\n  Number of thumbnails: " << *number_of_thumbnails
                  << "\n  File type: " << file_type
                  << "\n  Download result: " << download_result;
}

}  // namespace ash::multidevice
