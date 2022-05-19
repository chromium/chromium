// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_

#include "ash/components/multidevice/remote_device_ref.h"
#include "ash/components/phonehub/camera_roll_manager.h"
#include "ash/components/phonehub/combined_access_setup_operation.h"
#include "ash/components/phonehub/multidevice_feature_access_manager.h"
#include "ash/components/phonehub/notification_access_setup_operation.h"
#include "ash/components/phonehub/util/histogram_util.h"
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-forward.h"
#include "ash/webui/eche_app_ui/apps_access_manager.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/android_sms/android_sms_app_manager.h"
#include "chrome/browser/ash/android_sms/android_sms_service_factory.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace base {
class DictionaryValue;
}  // namespace base

namespace chromeos {
namespace settings {

// Chrome "Multidevice" (a.k.a. "Connected Devices") settings page UI handler.
class MultideviceHandler
    : public ::settings::SettingsPageUIHandler,
      public multidevice_setup::MultiDeviceSetupClient::Observer,
      public multidevice_setup::AndroidSmsPairingStateTracker::Observer,
      public android_sms::AndroidSmsAppManager::Observer,
      public phonehub::MultideviceFeatureAccessManager::Observer,
      public phonehub::NotificationAccessSetupOperation::Delegate,
      public ash::eche_app::AppsAccessManager::Observer,
      public ash::eche_app::AppsAccessSetupOperation::Delegate,
      public ash::phonehub::CameraRollManager::Observer,
      public phonehub::CombinedAccessSetupOperation::Delegate {
 public:
  MultideviceHandler(
      PrefService* prefs,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      phonehub::MultideviceFeatureAccessManager*
          multidevice_feature_access_manager,
      multidevice_setup::AndroidSmsPairingStateTracker*
          android_sms_pairing_state_tracker,
      android_sms::AndroidSmsAppManager* android_sms_app_manager,
      ash::eche_app::AppsAccessManager* apps_access_manager,
      ash::phonehub::CameraRollManager* camera_roll_manager);

  MultideviceHandler(const MultideviceHandler&) = delete;
  MultideviceHandler& operator=(const MultideviceHandler&) = delete;

  ~MultideviceHandler() override;

 protected:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void LogPhoneHubPermissionSetUpScreenAction(const base::Value::List& args);
  void LogPhoneHubPermissionSetUpButtonClicked(const base::Value::List& args);

 private:
  // ::settings::SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_status_with_device) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  // NotificationAccessSetupOperation::Delegate:
  void OnNotificationStatusChange(
      phonehub::NotificationAccessSetupOperation::Status new_status) override;

  // ash::eche_app::AppsAccessSetupOperation::Delegate:
  void OnAppsStatusChange(
      ash::eche_app::AppsAccessSetupOperation::Status new_status) override;

  // CombinedAccessSetupOperation::Delegate:
  void OnCombinedStatusChange(
      phonehub::CombinedAccessSetupOperation::Status new_status) override;

  // phonehub::MultideviceFeatureAccessManager::Observer:
  void OnNotificationAccessChanged() override;
  void OnCameraRollAccessChanged() override;
  void OnFeatureSetupRequestSupportedChanged() override;

  // multidevice_setup::AndroidSmsPairingStateTracker::Observer:
  void OnPairingStateChanged() override;

  // android_sms::AndroidSmsAppManager::Observer:
  void OnInstalledAppUrlChanged() override;

  // ash::eche_app::AppsAccessManager::Observer:
  void OnAppsAccessChanged() override;

  // ash::phonehub::CameraRollManager::Observer:
  void OnCameraRollViewUiStateUpdated() override;

  // Called when the Nearby Share enabled pref changes.
  void OnNearbySharingEnabledChanged();

  // Chromeos screen lock enabled pref change observer.
  void OnEnableScreenLockChanged();

  // Phone screen lock status pref change observer.
  void OnScreenLockStatusChanged();

  // Sends the most recent PageContentData dictionary to the WebUI page as an
  // update (e.g., not due to a getPageContent() request).
  void UpdatePageContent();

  void HandleShowMultiDeviceSetupDialog(const base::Value::List& args);
  void HandleGetPageContent(const base::Value::List& args);
  void HandleSetFeatureEnabledState(const base::Value::List& args);
  void HandleRemoveHostDevice(const base::Value::List& args);
  void HandleRetryPendingHostSetup(const base::Value::List& args);
  void HandleSetUpAndroidSms(const base::Value::List& args);
  void HandleGetSmartLockSignInEnabled(const base::Value::List& args);
  void HandleSetSmartLockSignInEnabled(const base::Value::List& args);
  void HandleGetSmartLockSignInAllowed(const base::Value::List& args);
  void HandleGetAndroidSmsInfo(const base::Value::List& args);
  void HandleAttemptNotificationSetup(const base::Value::List& args);
  void HandleCancelNotificationSetup(const base::Value::List& args);
  void HandleAttemptAppsSetup(const base::Value::List& args);
  void HandleCancelAppsSetup(const base::Value::List& args);
  void HandleAttemptCombinedFeatureSetup(const base::Value::List& args);
  void HandleCancelCombinedFeatureSetup(const base::Value::List& args);

  void OnSetFeatureStateEnabledResult(const std::string& js_callback_id,
                                      bool success);

  void NotifySmartLockSignInEnabledChanged();
  void NotifySmartLockSignInAllowedChanged();
  // Generate android sms info dictionary containing the messages for web
  // content settings origin url and messages feature state.
  std::unique_ptr<base::DictionaryValue> GenerateAndroidSmsInfo();
  void NotifyAndroidSmsInfoChange();

  // Returns true if |auth_token| matches the current auth token stored in
  // QuickUnlockStorage, i.e., the user has successfully authenticated recently.
  bool IsAuthTokenValid(const std::string& auth_token);

  // Unowned pointer to the preferences service.
  PrefService* prefs_;

  // Registers preference value change listeners.
  PrefChangeRegistrar pref_change_registrar_;

  // Returns null if requisite data has not yet been fetched (i.e., if one or
  // both of |last_host_status_update_| and |last_feature_states_update_| is
  // null).
  std::unique_ptr<base::DictionaryValue> GeneratePageContentDataDictionary();

  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
  GetHostStatusWithDevice();
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
  GetFeatureStatesMap();

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;

  phonehub::MultideviceFeatureAccessManager*
      multidevice_feature_access_manager_;
  std::unique_ptr<phonehub::NotificationAccessSetupOperation>
      notification_access_operation_;
  std::unique_ptr<phonehub::CombinedAccessSetupOperation>
      combined_access_operation_;

  multidevice_setup::AndroidSmsPairingStateTracker*
      android_sms_pairing_state_tracker_;
  android_sms::AndroidSmsAppManager* android_sms_app_manager_;

  ash::eche_app::AppsAccessManager* apps_access_manager_;
  std::unique_ptr<ash::eche_app::AppsAccessSetupOperation>
      apps_access_operation_;

  ash::phonehub::CameraRollManager* camera_roll_manager_;

  base::ScopedObservation<multidevice_setup::MultiDeviceSetupClient,
                          multidevice_setup::MultiDeviceSetupClient::Observer>
      multidevice_setup_observation_{this};
  base::ScopedObservation<
      multidevice_setup::AndroidSmsPairingStateTracker,
      multidevice_setup::AndroidSmsPairingStateTracker::Observer>
      android_sms_pairing_state_tracker_observation_{this};
  base::ScopedObservation<android_sms::AndroidSmsAppManager,
                          android_sms::AndroidSmsAppManager::Observer>
      android_sms_app_manager_observation_{this};
  base::ScopedObservation<phonehub::MultideviceFeatureAccessManager,
                          phonehub::MultideviceFeatureAccessManager::Observer>
      multidevice_feature_access_manager_observation_{this};
  base::ScopedObservation<ash::eche_app::AppsAccessManager,
                          ash::eche_app::AppsAccessManager::Observer>
      apps_access_manager_observation_{this};
  base::ScopedObservation<ash::phonehub::CameraRollManager,
                          ash::phonehub::CameraRollManager::Observer>
      camera_roll_manager_observation_{this};

  // Used to cancel callbacks when JavaScript becomes disallowed.
  base::WeakPtrFactory<MultideviceHandler> callback_weak_ptr_factory_{this};
};

}  // namespace settings

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_
