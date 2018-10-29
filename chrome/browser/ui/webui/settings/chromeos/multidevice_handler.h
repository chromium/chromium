// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace base {
class DictionaryValue;
}  // namespace base

namespace chromeos {

namespace multidevice_setup {
class AndroidSmsAppHelperDelegate;
}  // namespace multidevice_setup

namespace settings {

// Chrome "Multidevice" (a.k.a. "Connected Devices") settings page UI handler.
class MultideviceHandler
    : public ::settings::SettingsPageUIHandler,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  MultideviceHandler(
      PrefService* prefs,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      std::unique_ptr<multidevice_setup::AndroidSmsAppHelperDelegate>
          android_sms_app_helper);
  ~MultideviceHandler() override;

 protected:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

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

  // Sends the most recent PageContentData dictionary to the WebUI page as an
  // update (e.g., not due to a getPageContent() request).
  void UpdatePageContent();

  void HandleShowMultiDeviceSetupDialog(const base::ListValue* args);
  void HandleGetPageContent(const base::ListValue* args);
  void HandleSetFeatureEnabledState(const base::ListValue* args);
  void HandleRemoveHostDevice(const base::ListValue* args);
  void HandleRetryPendingHostSetup(const base::ListValue* args);
  void HandleSetUpAndroidSms(const base::ListValue* args);
  void HandleGetSmartLockSignInEnabled(const base::ListValue* args);
  void HandleSetSmartLockSignInEnabled(const base::ListValue* args);
  void HandleGetSmartLockSignInAllowed(const base::ListValue* args);
  void HandleGetAndroidSmsInfo(const base::ListValue* args);

  void OnSetFeatureStateEnabledResult(const std::string& js_callback_id,
                                      bool success);

  void RegisterPrefChangeListeners();
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
  std::unique_ptr<multidevice_setup::AndroidSmsAppHelperDelegate>
      android_sms_app_helper_;

  ScopedObserver<multidevice_setup::MultiDeviceSetupClient,
                 multidevice_setup::MultiDeviceSetupClient::Observer>
      multidevice_setup_observer_;

  // Used to cancel callbacks when JavaScript becomes disallowed.
  base::WeakPtrFactory<MultideviceHandler> callback_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MultideviceHandler);
};

}  // namespace settings

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_
