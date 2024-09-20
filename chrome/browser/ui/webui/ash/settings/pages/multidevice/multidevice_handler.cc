// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/multidevice/multidevice_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/phonehub/browser_tabs_model_provider_impl.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_status.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model_provider.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/components/phonehub/screen_lock_manager.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash::settings {

using phonehub::util::LogPermissionOnboardingDialogAction;
using phonehub::util::LogPermissionOnboardingSettingsClicked;
using phonehub::util::LogPermissionOnboardingSetupMode;
using phonehub::util::LogPermissionOnboardingSetupResult;
using phonehub::util::PermissionsOnboardingScreenEvent;
using phonehub::util::PermissionsOnboardingSetUpMode;
using phonehub::util::PermissionsOnboardingStep;

namespace {

const char kCameraRollAccessStatus[] = "cameraRollAccessStatus";

const char kPageContentDataModeKey[] = "mode";
const char kPageContentDataHostDeviceNameKey[] = "hostDeviceName";
const char kPageContentDataBetterTogetherStateKey[] = "betterTogetherState";
const char kPageContentDataInstantTetheringStateKey[] = "instantTetheringState";
const char kPageContentDataPhoneHubStateKey[] = "phoneHubState";
const char kPageContentDataPhoneHubCameraRollStateKey[] =
    "phoneHubCameraRollState";
const char kPageContentDataPhoneHubNotificationsStateKey[] =
    "phoneHubNotificationsState";
const char kPageContentDataPhoneHubTaskContinuationStateKey[] =
    "phoneHubTaskContinuationState";
const char kPageContentDataPhoneHubAppsStateKey[] = "phoneHubAppsState";
const char kPageContentDataWifiSyncStateKey[] = "wifiSyncState";
const char kPageContentDataSmartLockStateKey[] = "smartLockState";
const char kNotificationAccessStatus[] = "notificationAccessStatus";
const char kNotificationAccessProhibitedReason[] =
    "notificationAccessProhibitedReason";
const char kIsNearbyShareDisallowedByPolicy[] =
    "isNearbyShareDisallowedByPolicy";
const char kAppsAccessStatus[] = "appsAccessStatus";
const char kIsPhoneHubPermissionsDialogSupported[] =
    "isPhoneHubPermissionsDialogSupported";
const char kIsCameraRollFilePermissionGranted[] =
    "isCameraRollFilePermissionGranted";
const char kIsPhoneHubFeatureCombinedSetupSupported[] =
    "isPhoneHubFeatureCombinedSetupSupported";
const char kIsChromeOSSyncedSessionSharingEnabled[] =
    "isChromeOSSyncedSessionSharingEnabled";
const char kIsLacrosTabSyncEnabled[] = "isLacrosTabSyncEnabled";

void OnRetrySetHostNowResult(bool success) {
  if (success) {
    return;
  }

  PA_LOG(WARNING) << "OnRetrySetHostNowResult(): Attempt to retry setting the "
                  << "host device failed.";
}

}  // namespace

MultideviceHandler::MultideviceHandler(
    PrefService* prefs,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    phonehub::MultideviceFeatureAccessManager*
        multidevice_feature_access_manager,
    eche_app::AppsAccessManager* apps_access_manager,
    phonehub::CameraRollManager* camera_roll_manager,
    phonehub::BrowserTabsModelProvider* browser_tabs_model_provider)
    : prefs_(prefs),
      multidevice_setup_client_(multidevice_setup_client),
      multidevice_feature_access_manager_(multidevice_feature_access_manager),
      apps_access_manager_(apps_access_manager),
      camera_roll_manager_(camera_roll_manager),
      browser_tabs_model_provider_(browser_tabs_model_provider) {
  pref_change_registrar_.Init(prefs_);
}

MultideviceHandler::~MultideviceHandler() = default;

void MultideviceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showMultiDeviceSetupDialog",
      base::BindRepeating(&MultideviceHandler::HandleShowMultiDeviceSetupDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPageContentData",
      base::BindRepeating(&MultideviceHandler::HandleGetPageContent,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setFeatureEnabledState",
      base::BindRepeating(&MultideviceHandler::HandleSetFeatureEnabledState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeHostDevice",
      base::BindRepeating(&MultideviceHandler::HandleRemoveHostDevice,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "retryPendingHostSetup",
      base::BindRepeating(&MultideviceHandler::HandleRetryPendingHostSetup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "attemptNotificationSetup",
      base::BindRepeating(&MultideviceHandler::HandleAttemptNotificationSetup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelNotificationSetup",
      base::BindRepeating(&MultideviceHandler::HandleCancelNotificationSetup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "attemptAppsSetup",
      base::BindRepeating(&MultideviceHandler::HandleAttemptAppsSetup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelAppsSetup",
      base::BindRepeating(&MultideviceHandler::HandleCancelAppsSetup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "attemptCombinedFeatureSetup",
      base::BindRepeating(
          &MultideviceHandler::HandleAttemptCombinedFeatureSetup,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelCombinedFeatureSetup",
      base::BindRepeating(&MultideviceHandler::HandleCancelCombinedFeatureSetup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "attemptFeatureSetupConnection",
      base::BindRepeating(
          &MultideviceHandler::HandleAttemptFeatureSetupConnection,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelFeatureSetupConnection",
      base::BindRepeating(
          &MultideviceHandler::HandleCancelFeatureSetupConnection,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showBrowserSyncSettings",
      base::BindRepeating(&MultideviceHandler::HandleShowBrowserSyncSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "logPhoneHubPermissionSetUpScreenAction",
      base::BindRepeating(
          &MultideviceHandler::LogPhoneHubPermissionSetUpScreenAction,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "logPhoneHubPermissionSetUpButtonClicked",
      base::BindRepeating(
          &MultideviceHandler::LogPhoneHubPermissionSetUpButtonClicked,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "logPhoneHubPermissionOnboardingSetupMode",
      base::BindRepeating(
          &MultideviceHandler::LogPhoneHubPermissionOnboardingSetupMode,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "logPhoneHubPermissionOnboardingSetupResult",
      base::BindRepeating(
          &MultideviceHandler::LogPhoneHubPermissionOnboardingSetupResult,
          base::Unretained(this)));
}

void MultideviceHandler::OnJavascriptAllowed() {
  if (multidevice_setup_client_) {
    multidevice_setup_observation_.Observe(multidevice_setup_client_.get());
  }

  if (multidevice_feature_access_manager_) {
    multidevice_feature_access_manager_observation_.Observe(
        multidevice_feature_access_manager_.get());
  }

  if (apps_access_manager_) {
    apps_access_manager_observation_.Observe(apps_access_manager_.get());
  }

  if (camera_roll_manager_) {
    camera_roll_manager_observation_.Observe(camera_roll_manager_.get());
  }

  if (NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          Profile::FromWebUI(web_ui()))) {
    pref_change_registrar_.Add(
        ::prefs::kNearbySharingEnabledPrefName,
        base::BindRepeating(&MultideviceHandler::OnNearbySharingEnabledChanged,
                            base::Unretained(this)));
  }
  if (features::IsEcheSWAEnabled()) {
    pref_change_registrar_.Add(
        ash::prefs::kEnableAutoScreenLock,
        base::BindRepeating(&MultideviceHandler::OnEnableScreenLockChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        phonehub::prefs::kScreenLockStatus,
        base::BindRepeating(&MultideviceHandler::OnScreenLockStatusChanged,
                            base::Unretained(this)));
  }
}

void MultideviceHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.RemoveAll();

  if (multidevice_setup_client_) {
    DCHECK(multidevice_setup_observation_.IsObservingSource(
        multidevice_setup_client_.get()));
    multidevice_setup_observation_.Reset();
  }

  if (multidevice_feature_access_manager_) {
    DCHECK(multidevice_feature_access_manager_observation_.IsObservingSource(
        multidevice_feature_access_manager_.get()));
    multidevice_feature_access_manager_observation_.Reset();
    notification_access_operation_.reset();
  }

  if (apps_access_manager_) {
    DCHECK(apps_access_manager_observation_.IsObservingSource(
        apps_access_manager_.get()));
    apps_access_manager_observation_.Reset();
    apps_access_operation_.reset();
  }

  if (camera_roll_manager_) {
    DCHECK(camera_roll_manager_observation_.IsObservingSource(
        camera_roll_manager_.get()));
    camera_roll_manager_observation_.Reset();
  }

  // Ensure that pending callbacks do not complete and cause JS to be evaluated.
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

void MultideviceHandler::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device) {
  UpdatePageContent();
}

void MultideviceHandler::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  PA_LOG(INFO) << "Feature states have changed: "
               << multidevice_setup::FeatureStatesMapToString(
                      feature_states_map);
  UpdatePageContent();
}

void MultideviceHandler::OnNotificationAccessChanged() {
  UpdatePageContent();
}

void MultideviceHandler::OnCameraRollAccessChanged() {
  UpdatePageContent();
}

void MultideviceHandler::OnFeatureSetupRequestSupportedChanged() {
  UpdatePageContent();
}

void MultideviceHandler::OnAppsAccessChanged() {
  UpdatePageContent();
}

void MultideviceHandler::OnCameraRollViewUiStateUpdated() {
  UpdatePageContent();
}

void MultideviceHandler::OnBrowserTabsUpdated(
    bool is_sync_enabled,
    const std::vector<phonehub::BrowserTabsModel::BrowserTabMetadata>&
        browser_tabs_metadata) {
  UpdatePageContent();
}

void MultideviceHandler::OnNearbySharingEnabledChanged() {
  UpdatePageContent();
}

void MultideviceHandler::UpdatePageContent() {
  base::Value::Dict page_content_dictionary =
      GeneratePageContentDataDictionary();
  PA_LOG(INFO) << "Updating MultiDevice settings page content with: "
               << page_content_dictionary << ".";
  FireWebUIListener("settings.updateMultidevicePageContentData",
                    page_content_dictionary);
}

void MultideviceHandler::HandleShowMultiDeviceSetupDialog(
    const base::Value::List& args) {
  DCHECK(args.empty());
  multidevice_setup::MultiDeviceSetupDialog::Show();
  ash::phonehub::util::LogMultiDeviceSetupDialogEntryPoint(
      ash::phonehub::util::MultiDeviceSetupDialogEntrypoint::kSettingsPage);
}

void MultideviceHandler::HandleGetPageContent(const base::Value::List& args) {
  // This callback is expected to be the first one executed when the page is
  // loaded, so it should be the one to allow JS calls.
  AllowJavascript();

  const base::Value& callback_id = args[0];
  DCHECK(callback_id.is_string());

  base::Value::Dict page_content_dictionary =
      GeneratePageContentDataDictionary();
  PA_LOG(INFO) << "Responding to getPageContentData() request with: "
               << page_content_dictionary << ".";

  ResolveJavascriptCallback(callback_id, page_content_dictionary);
}

void MultideviceHandler::HandleSetFeatureEnabledState(
    const base::Value::List& args) {
  const auto& list = args;
  DCHECK_GE(list.size(), 3u);
  std::string callback_id = list[0].GetString();

  int feature_as_int = list[1].GetInt();

  auto feature = static_cast<multidevice_setup::mojom::Feature>(feature_as_int);
  DCHECK(multidevice_setup::mojom::IsKnownEnumValue(feature));

  bool enabled = list[2].GetBool();

  std::optional<std::string> auth_token;
  if (list.size() >= 4 && list[3].is_string()) {
    auth_token = list[3].GetString();
  }

  multidevice_setup_client_->SetFeatureEnabledState(
      feature, enabled, auth_token,
      base::BindOnce(&MultideviceHandler::OnSetFeatureStateEnabledResult,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));

  if (enabled && feature == multidevice_setup::mojom::Feature::kPhoneHub) {
    phonehub::util::LogFeatureOptInEntryPoint(
        phonehub::util::OptInEntryPoint::kSettings);
  }

  if (enabled &&
      feature == multidevice_setup::mojom::Feature::kPhoneHubCameraRoll) {
    phonehub::util::LogCameraRollFeatureOptInEntryPoint(
        phonehub::util::CameraRollOptInEntryPoint::kSettings);
  }
}

void MultideviceHandler::HandleRemoveHostDevice(const base::Value::List& args) {
  DCHECK(args.empty());
  multidevice_setup_client_->RemoveHostDevice();
}

void MultideviceHandler::HandleRetryPendingHostSetup(
    const base::Value::List& args) {
  DCHECK(args.empty());
  multidevice_setup_client_->RetrySetHostNow(
      base::BindOnce(&OnRetrySetHostNowResult));
}

void MultideviceHandler::HandleAttemptNotificationSetup(
    const base::Value::List& args) {
  DCHECK(features::IsPhoneHubEnabled());
  DCHECK(!notification_access_operation_);

  phonehub::MultideviceFeatureAccessManager::AccessStatus
      notification_access_status =
          multidevice_feature_access_manager_->GetNotificationAccessStatus();
  if (notification_access_status != phonehub::MultideviceFeatureAccessManager::
                                        AccessStatus::kAvailableButNotGranted) {
    PA_LOG(WARNING) << "Cannot request notification access setup flow; current "
                    << "status: " << notification_access_status;
    return;
  }

  notification_access_operation_ =
      multidevice_feature_access_manager_->AttemptNotificationSetup(
          /*delegate=*/this);
  DCHECK(notification_access_operation_);
}

void MultideviceHandler::HandleCancelNotificationSetup(
    const base::Value::List& args) {
  DCHECK(features::IsPhoneHubEnabled());
  DCHECK(notification_access_operation_);

  notification_access_operation_.reset();
}

void MultideviceHandler::HandleAttemptAppsSetup(const base::Value::List& args) {
  DCHECK(features::IsEcheSWAEnabled());
  DCHECK(!apps_access_operation_);

  phonehub::MultideviceFeatureAccessManager::AccessStatus apps_access_status =
      apps_access_manager_->GetAccessStatus();

  if (apps_access_status != phonehub::MultideviceFeatureAccessManager::
                                AccessStatus::kAvailableButNotGranted) {
    PA_LOG(WARNING) << "Cannot request apps access setup flow; current "
                    << "status: " << apps_access_status;
    return;
  }

  apps_access_operation_ =
      apps_access_manager_->AttemptAppsAccessSetup(/*delegate=*/this);
  DCHECK(apps_access_operation_);
}

void MultideviceHandler::HandleCancelAppsSetup(const base::Value::List& args) {
  DCHECK(features::IsEcheSWAEnabled());
  DCHECK(apps_access_operation_);

  apps_access_manager_->NotifyAppsAccessCanceled();
  apps_access_operation_.reset();
}

void MultideviceHandler::HandleAttemptCombinedFeatureSetup(
    const base::Value::List& args) {
  bool camera_roll = false;
  if (args[0].is_bool()) {
    camera_roll = args[0].GetBool();
  }
  bool notifications = false;
  if (args[1].is_bool()) {
    notifications = args[1].GetBool();
  }

  DCHECK(features::IsPhoneHubEnabled());
  DCHECK(!combined_access_operation_);

  if (!multidevice_feature_access_manager_->GetFeatureSetupRequestSupported()) {
    PA_LOG(WARNING) << "Cannot request combined access setup flow; "
                    << "FeatureSetupRequest is not supported by the phone.";
    return;
  }

  phonehub::MultideviceFeatureAccessManager::AccessStatus
      notification_access_status =
          multidevice_feature_access_manager_->GetNotificationAccessStatus();
  phonehub::MultideviceFeatureAccessManager::AccessStatus
      camera_roll_access_status =
          multidevice_feature_access_manager_->GetCameraRollAccessStatus();
  if (camera_roll_access_status != phonehub::MultideviceFeatureAccessManager::
                                       AccessStatus::kAvailableButNotGranted &&
      camera_roll) {
    PA_LOG(WARNING) << "Cannot request combined access setup flow; current "
                    << "Camera Roll status: " << camera_roll_access_status;
    return;
  }
  if (notification_access_status != phonehub::MultideviceFeatureAccessManager::
                                        AccessStatus::kAvailableButNotGranted &&
      notifications) {
    PA_LOG(WARNING) << "Cannot request combined access setup flow; current "
                    << "Notification status: " << notification_access_status;
    return;
  }

  combined_access_operation_ =
      multidevice_feature_access_manager_->AttemptCombinedFeatureSetup(
          camera_roll, notifications, /*delegate=*/this);
  DCHECK(combined_access_operation_);
}

void MultideviceHandler::HandleCancelCombinedFeatureSetup(
    const base::Value::List& args) {
  DCHECK(features::IsPhoneHubEnabled());
  DCHECK(combined_access_operation_);

  combined_access_operation_.reset();
}

void MultideviceHandler::HandleAttemptFeatureSetupConnection(
    const base::Value::List& args) {
  DCHECK(features::IsPhoneHubEnabled());
  DCHECK(!feature_setup_connection_operation_);

  feature_setup_connection_operation_ =
      multidevice_feature_access_manager_->AttemptFeatureSetupConnection(this);
  DCHECK(feature_setup_connection_operation_);
}

void MultideviceHandler::HandleCancelFeatureSetupConnection(
    const base::Value::List& args) {
  DCHECK(features::IsPhoneHubEnabled());
  DCHECK(feature_setup_connection_operation_);

  feature_setup_connection_operation_.reset();
}

void MultideviceHandler::HandleShowBrowserSyncSettings(
    const base::Value::List& args) {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL("chrome://settings/syncSetup/advanced"),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kSwitchToTab);
}

void MultideviceHandler::LogPhoneHubPermissionSetUpScreenAction(
    const base::Value::List& args) {
  int setup_screen_int = args[0].GetInt();
  int setup_action_int = args[1].GetInt();
  LogPermissionOnboardingDialogAction(
      static_cast<PermissionsOnboardingStep>(setup_screen_int),
      static_cast<PermissionsOnboardingScreenEvent>(setup_action_int));
}

void MultideviceHandler::LogPhoneHubPermissionSetUpButtonClicked(
    const base::Value::List& args) {
  int setup_mode = args[0].GetInt();
  LogPermissionOnboardingSettingsClicked(
      static_cast<PermissionsOnboardingSetUpMode>(setup_mode));
}

void MultideviceHandler::LogPhoneHubPermissionOnboardingSetupMode(
    const base::Value::List& args) {
  int setup_mode = args[0].GetInt();
  LogPermissionOnboardingSetupMode(
      static_cast<PermissionsOnboardingSetUpMode>(setup_mode));
}

void MultideviceHandler::LogPhoneHubPermissionOnboardingSetupResult(
    const base::Value::List& args) {
  int completed_mode = args[0].GetInt();
  LogPermissionOnboardingSetupResult(
      static_cast<PermissionsOnboardingSetUpMode>(completed_mode));
}

void MultideviceHandler::OnNotificationStatusChange(
    phonehub::NotificationAccessSetupOperation::Status new_status) {
  FireWebUIListener("settings.onNotificationAccessSetupStatusChanged",
                    base::Value(static_cast<int32_t>(new_status)));

  if (phonehub::NotificationAccessSetupOperation::IsFinalStatus(new_status)) {
    notification_access_operation_.reset();
  }
}

void MultideviceHandler::OnAppsStatusChange(
    eche_app::AppsAccessSetupOperation::Status new_status) {
  FireWebUIListener("settings.onAppsAccessSetupStatusChanged",
                    base::Value(static_cast<int32_t>(new_status)));

  if (eche_app::AppsAccessSetupOperation::IsFinalStatus(new_status)) {
    apps_access_operation_.reset();
  }
}

void MultideviceHandler::OnCombinedStatusChange(
    phonehub::CombinedAccessSetupOperation::Status new_status) {
  FireWebUIListener("settings.onCombinedAccessSetupStatusChanged",
                    base::Value(static_cast<int32_t>(new_status)));

  if (phonehub::CombinedAccessSetupOperation::IsFinalStatus(new_status)) {
    combined_access_operation_.reset();
  }
}

void MultideviceHandler::OnFeatureSetupConnectionStatusChange(
    phonehub::FeatureSetupConnectionOperation::Status new_status) {
  FireWebUIListener("settings.onFeatureSetupConnectionStatusChanged",
                    base::Value(static_cast<int32_t>(new_status)));

  if (phonehub::FeatureSetupConnectionOperation::IsFinalStatus(new_status)) {
    feature_setup_connection_operation_.reset();
  }
}

void MultideviceHandler::OnSetFeatureStateEnabledResult(
    const std::string& js_callback_id,
    bool success) {
  ResolveJavascriptCallback(base::Value(js_callback_id), base::Value(success));
}

base::Value::Dict MultideviceHandler::GeneratePageContentDataDictionary() {
  base::Value::Dict page_content_dictionary;

  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
      host_status_with_device = GetHostStatusWithDevice();
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap feature_states =
      GetFeatureStatesMap();

  page_content_dictionary.Set(
      kPageContentDataModeKey,
      static_cast<int32_t>(host_status_with_device.first));
  page_content_dictionary.Set(
      kPageContentDataBetterTogetherStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kBetterTogetherSuite]));
  page_content_dictionary.Set(
      kPageContentDataInstantTetheringStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kInstantTethering]));
  page_content_dictionary.Set(
      kPageContentDataSmartLockStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kSmartLock]));
  page_content_dictionary.Set(
      kPageContentDataPhoneHubStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kPhoneHub]));
  auto cameraRoll_feature_state =
      base::FeatureList::IsEnabled(ash::features::kPhoneHubCameraRoll)
          ? feature_states
                [multidevice_setup::mojom::Feature::kPhoneHubCameraRoll]
          : multidevice_setup::mojom::FeatureState::kNotSupportedByChromebook;
  page_content_dictionary.Set(kPageContentDataPhoneHubCameraRollStateKey,
                              static_cast<int32_t>(cameraRoll_feature_state));
  page_content_dictionary.Set(
      kPageContentDataPhoneHubNotificationsStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kPhoneHubNotifications]));
  page_content_dictionary.Set(
      kPageContentDataPhoneHubTaskContinuationStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kPhoneHubTaskContinuation]));
  auto eche_feature_state =
      base::FeatureList::IsEnabled(ash::features::kEcheSWA)
          ? feature_states[multidevice_setup::mojom::Feature::kEche]
          : multidevice_setup::mojom::FeatureState::kNotSupportedByChromebook;
  page_content_dictionary.Set(kPageContentDataPhoneHubAppsStateKey,
                              static_cast<int32_t>(eche_feature_state));

  page_content_dictionary.Set(
      kPageContentDataWifiSyncStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kWifiSync]));

  if (host_status_with_device.second) {
    page_content_dictionary.Set(kPageContentDataHostDeviceNameKey,
                                host_status_with_device.second->name());
  }

  phonehub::MultideviceFeatureAccessManager::AccessStatus
      notification_access_status = phonehub::MultideviceFeatureAccessManager::
          AccessStatus::kAvailableButNotGranted;
  phonehub::MultideviceFeatureAccessManager::AccessProhibitedReason reason =
      phonehub::MultideviceFeatureAccessManager::AccessProhibitedReason::
          kUnknown;
  if (multidevice_feature_access_manager_) {
    notification_access_status =
        multidevice_feature_access_manager_->GetNotificationAccessStatus();
    reason = multidevice_feature_access_manager_
                 ->GetNotificationAccessProhibitedReason();
  }

  page_content_dictionary.Set(kNotificationAccessStatus,
                              static_cast<int32_t>(notification_access_status));
  page_content_dictionary.Set(kNotificationAccessProhibitedReason,
                              static_cast<int32_t>(reason));

  phonehub::MultideviceFeatureAccessManager::AccessStatus
      camera_roll_access_status = phonehub::MultideviceFeatureAccessManager::
          AccessStatus::kAvailableButNotGranted;
  if (multidevice_feature_access_manager_) {
    camera_roll_access_status =
        multidevice_feature_access_manager_->GetCameraRollAccessStatus();
  }
  page_content_dictionary.Set(kCameraRollAccessStatus,
                              static_cast<int32_t>(camera_roll_access_status));

  phonehub::MultideviceFeatureAccessManager::AccessStatus apps_access_status =
      phonehub::MultideviceFeatureAccessManager::AccessStatus::
          kAvailableButNotGranted;
  if (apps_access_manager_) {
    apps_access_status = apps_access_manager_->GetAccessStatus();
  }

  page_content_dictionary.Set(kAppsAccessStatus,
                              static_cast<int32_t>(apps_access_status));

  bool is_camera_roll_file_permission_granted = false;
  if (camera_roll_manager_) {
    is_camera_roll_file_permission_granted =
        camera_roll_manager_->ui_state() !=
        phonehub::CameraRollManager::CameraRollUiState::NO_STORAGE_PERMISSION;
  }
  page_content_dictionary.Set(kIsCameraRollFilePermissionGranted,
                              is_camera_roll_file_permission_granted);

  bool is_nearby_share_disallowed_by_policy =
      NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          Profile::FromWebUI(web_ui())) &&
      (GetNearbyShareEnabledState(prefs_) ==
       NearbyShareEnabledState::kDisallowedByPolicy);
  page_content_dictionary.Set(kIsNearbyShareDisallowedByPolicy,
                              is_nearby_share_disallowed_by_policy);

  bool is_phone_hub_permissions_dialog_supported =
      features::IsEcheSWAEnabled() || features::IsPhoneHubCameraRollEnabled();
  page_content_dictionary.Set(kIsPhoneHubPermissionsDialogSupported,
                              is_phone_hub_permissions_dialog_supported);

  page_content_dictionary.Set(kIsPhoneHubFeatureCombinedSetupSupported,
                              multidevice_feature_access_manager_
                                  ? multidevice_feature_access_manager_
                                        ->GetFeatureSetupRequestSupported()
                                  : false);

  page_content_dictionary.Set(kIsChromeOSSyncedSessionSharingEnabled, false);
  page_content_dictionary.Set(kIsLacrosTabSyncEnabled, false);

  return page_content_dictionary;
}

bool MultideviceHandler::IsAuthTokenValid(const std::string& auth_token) {
  return ash::AuthSessionStorage::Get()->IsValid(auth_token);
}

multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
MultideviceHandler::GetHostStatusWithDevice() {
  if (multidevice_setup_client_) {
    return multidevice_setup_client_->GetHostStatus();
  }

  return multidevice_setup::MultiDeviceSetupClient::
      GenerateDefaultHostStatusWithDevice();
}

multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
MultideviceHandler::GetFeatureStatesMap() {
  if (multidevice_setup_client_) {
    return multidevice_setup_client_->GetFeatureStates();
  }

  PA_LOG(WARNING)
      << "MultiDevice setup client missing. Responding to "
         "GetFeatureStatesMap() request by generating default feature map.";
  return multidevice_setup::MultiDeviceSetupClient::
      GenerateDefaultFeatureStatesMap(
          multidevice_setup::mojom::FeatureState::kProhibitedByPolicy);
}

void MultideviceHandler::OnEnableScreenLockChanged() {
  // We need to use FireWebUIListener to update value dynamically because
  // loadTimeData is not recreated on refresh.
  const bool is_screen_lock_enabled =
      SessionControllerClientImpl::CanLockScreen() &&
      SessionControllerClientImpl::ShouldLockScreenAutomatically();
  FireWebUIListener("settings.OnEnableScreenLockChanged",
                    base::Value(is_screen_lock_enabled));
}

void MultideviceHandler::OnScreenLockStatusChanged() {
  // We need to use FireWebUIListener to update value dynamically because
  // loadTimeData is not recreated on refresh.
  const bool is_phone_screen_lock_enabled =
      static_cast<phonehub::ScreenLockManager::LockStatus>(
          prefs_->GetInteger(phonehub::prefs::kScreenLockStatus)) ==
      phonehub::ScreenLockManager::LockStatus::kLockedOn;
  FireWebUIListener("settings.OnScreenLockStatusChanged",
                    base::Value(is_phone_screen_lock_enabled));
}

}  // namespace ash::settings
