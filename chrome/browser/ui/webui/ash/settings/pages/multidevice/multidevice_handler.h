// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MULTIDEVICE_MULTIDEVICE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MULTIDEVICE_MULTIDEVICE_HANDLER_H_

#include "ash/webui/eche_app_ui/apps_access_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model_provider.h"
#include "chromeos/ash/components/phonehub/camera_roll_manager.h"
#include "chromeos/ash/components/phonehub/combined_access_setup_operation.h"
#include "chromeos/ash/components/phonehub/feature_setup_connection_operation.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/notification_access_setup_operation.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-forward.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace ash::settings {

// Chrome "Multidevice" (a.k.a. "Connected Devices") settings page UI handler.
class MultideviceHandler
    : public ::settings::SettingsPageUIHandler,
      public multidevice_setup::MultiDeviceSetupClient::Observer,
      public phonehub::MultideviceFeatureAccessManager::Observer,
      public phonehub::NotificationAccessSetupOperation::Delegate,
      public eche_app::AppsAccessManager::Observer,
      public eche_app::AppsAccessSetupOperation::Delegate,
      public phonehub::CameraRollManager::Observer,
      public phonehub::CombinedAccessSetupOperation::Delegate,
      public phonehub::FeatureSetupConnectionOperation::Delegate,
      public phonehub::BrowserTabsModelProvider::Observer {
 public:
  MultideviceHandler(
      PrefService* prefs,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      phonehub::MultideviceFeatureAccessManager*
          multidevice_feature_access_manager,
      eche_app::AppsAccessManager* apps_access_manager,
      phonehub::CameraRollManager* camera_roll_manager,
      phonehub::BrowserTabsModelProvider* browser_tabs_model_provider);

  MultideviceHandler(const MultideviceHandler&) = delete;
  MultideviceHandler& operator=(const MultideviceHandler&) = delete;

  ~MultideviceHandler() override;

 protected:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void LogPhoneHubPermissionSetUpScreenAction(const base::Value::List& args);
  void LogPhoneHubPermissionSetUpButtonClicked(const base::Value::List& args);
  void LogPhoneHubPermissionOnboardingSetupMode(const base::Value::List& args);
  void LogPhoneHubPermissionOnboardingSetupResult(
      const base::Value::List& args);

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

  // eche_app::AppsAccessSetupOperation::Delegate:
  void OnAppsStatusChange(
      eche_app::AppsAccessSetupOperation::Status new_status) override;

  // CombinedAccessSetupOperation::Delegate:
  void OnCombinedStatusChange(
      phonehub::CombinedAccessSetupOperation::Status new_status) override;

  // FeatureSetupConnectionOperation::Delegate:
  void OnFeatureSetupConnectionStatusChange(
      phonehub::FeatureSetupConnectionOperation::Status new_status) override;

  // phonehub::MultideviceFeatureAccessManager::Observer:
  void OnNotificationAccessChanged() override;
  void OnCameraRollAccessChanged() override;
  void OnFeatureSetupRequestSupportedChanged() override;

  // eche_app::AppsAccessManager::Observer:
  void OnAppsAccessChanged() override;

  // phonehub::CameraRollManager::Observer:
  void OnCameraRollViewUiStateUpdated() override;

  // phonehub::BrowserTabsModelProvider::Observer:
  void OnBrowserTabsUpdated(
      bool is_sync_enabled,
      const std::vector<phonehub::BrowserTabsModel::BrowserTabMetadata>&
          browser_tabs_metadata) override;

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
  void HandleAttemptNotificationSetup(const base::Value::List& args);
  void HandleCancelNotificationSetup(const base::Value::List& args);
  void HandleAttemptAppsSetup(const base::Value::List& args);
  void HandleCancelAppsSetup(const base::Value::List& args);
  void HandleAttemptCombinedFeatureSetup(const base::Value::List& args);
  void HandleCancelCombinedFeatureSetup(const base::Value::List& args);
  void HandleAttemptFeatureSetupConnection(const base::Value::List& args);
  void HandleCancelFeatureSetupConnection(const base::Value::List& args);
  void HandleFinishFeatureSetupConnection(const base::Value::List& args);
  void HandleShowBrowserSyncSettings(const base::Value::List& args);

  void OnSetFeatureStateEnabledResult(const std::string& js_callback_id,
                                      bool success);

  // Returns true if |auth_token| matches the current auth token stored in
  // QuickUnlockStorage, i.e., the user has successfully authenticated recently.
  bool IsAuthTokenValid(const std::string& auth_token);

  // Unowned pointer to the preferences service.
  raw_ptr<PrefService> prefs_;

  // Registers preference value change listeners.
  PrefChangeRegistrar pref_change_registrar_;

  // Returns null if requisite data has not yet been fetched (i.e., if one or
  // both of |last_host_status_update_| and |last_feature_states_update_| is
  // null).
  base::Value::Dict GeneratePageContentDataDictionary();

  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
  GetHostStatusWithDevice();
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
  GetFeatureStatesMap();

  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;

  raw_ptr<phonehub::MultideviceFeatureAccessManager>
      multidevice_feature_access_manager_;
  std::unique_ptr<phonehub::NotificationAccessSetupOperation>
      notification_access_operation_;
  std::unique_ptr<phonehub::CombinedAccessSetupOperation>
      combined_access_operation_;
  std::unique_ptr<phonehub::FeatureSetupConnectionOperation>
      feature_setup_connection_operation_;

  raw_ptr<eche_app::AppsAccessManager> apps_access_manager_;
  std::unique_ptr<eche_app::AppsAccessSetupOperation> apps_access_operation_;

  raw_ptr<phonehub::CameraRollManager> camera_roll_manager_;
  raw_ptr<phonehub::BrowserTabsModelProvider> browser_tabs_model_provider_;

  base::ScopedObservation<multidevice_setup::MultiDeviceSetupClient,
                          multidevice_setup::MultiDeviceSetupClient::Observer>
      multidevice_setup_observation_{this};
  base::ScopedObservation<phonehub::MultideviceFeatureAccessManager,
                          phonehub::MultideviceFeatureAccessManager::Observer>
      multidevice_feature_access_manager_observation_{this};
  base::ScopedObservation<eche_app::AppsAccessManager,
                          eche_app::AppsAccessManager::Observer>
      apps_access_manager_observation_{this};
  base::ScopedObservation<phonehub::CameraRollManager,
                          phonehub::CameraRollManager::Observer>
      camera_roll_manager_observation_{this};
  base::ScopedObservation<phonehub::BrowserTabsModelProvider,
                          phonehub::BrowserTabsModelProvider::Observer>
      browser_tabs_model_provider_observation_{this};

  // Used to cancel callbacks when JavaScript becomes disallowed.
  base::WeakPtrFactory<MultideviceHandler> callback_weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_MULTIDEVICE_MULTIDEVICE_HANDLER_H_
