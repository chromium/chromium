// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/android_sms/android_sms_pairing_state_tracker_impl.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/chromeos/login/quick_unlock/auth_token.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/proximity_auth/proximity_auth_pref_names.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

namespace settings {

namespace {

const char kPageContentDataModeKey[] = "mode";
const char kPageContentDataHostDeviceNameKey[] = "hostDeviceName";
const char kPageContentDataBetterTogetherStateKey[] = "betterTogetherState";
const char kPageContentDataInstantTetheringStateKey[] = "instantTetheringState";
const char kPageContentDataMessagesStateKey[] = "messagesState";
const char kPageContentDataSmartLockStateKey[] = "smartLockState";
const char kIsAndroidSmsPairingComplete[] = "isAndroidSmsPairingComplete";

constexpr char kAndroidSmsInfoOriginKey[] = "origin";
constexpr char kAndroidSmsInfoEnabledKey[] = "enabled";

void OnRetrySetHostNowResult(bool success) {
  if (success)
    return;

  PA_LOG(WARNING) << "OnRetrySetHostNowResult(): Attempt to retry setting the "
                  << "host device failed.";
}

}  // namespace

MultideviceHandler::MultideviceHandler(
    PrefService* prefs,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    multidevice_setup::AndroidSmsPairingStateTracker*
        android_sms_pairing_state_tracker,
    android_sms::AndroidSmsAppManager* android_sms_app_manager)
    : prefs_(prefs),
      multidevice_setup_client_(multidevice_setup_client),
      android_sms_pairing_state_tracker_(android_sms_pairing_state_tracker),
      android_sms_app_manager_(android_sms_app_manager),
      multidevice_setup_observer_(this),
      android_sms_pairing_state_tracker_observer_(this),
      android_sms_app_manager_observer_(this) {
  pref_change_registrar_.Init(prefs_);
}

MultideviceHandler::~MultideviceHandler() {}

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
      "setUpAndroidSms",
      base::BindRepeating(&MultideviceHandler::HandleSetUpAndroidSms,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSmartLockSignInEnabled",
      base::BindRepeating(&MultideviceHandler::HandleGetSmartLockSignInEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setSmartLockSignInEnabled",
      base::BindRepeating(&MultideviceHandler::HandleSetSmartLockSignInEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSmartLockSignInAllowed",
      base::BindRepeating(&MultideviceHandler::HandleGetSmartLockSignInAllowed,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAndroidSmsInfo",
      base::BindRepeating(&MultideviceHandler::HandleGetAndroidSmsInfo,
                          base::Unretained(this)));
}

void MultideviceHandler::OnJavascriptAllowed() {
  if (multidevice_setup_client_)
    multidevice_setup_observer_.Add(multidevice_setup_client_);

  if (android_sms_pairing_state_tracker_) {
    android_sms_pairing_state_tracker_observer_.Add(
        android_sms_pairing_state_tracker_);
  }

  if (android_sms_app_manager_)
    android_sms_app_manager_observer_.Add(android_sms_app_manager_);

  pref_change_registrar_.Add(
      proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled,
      base::BindRepeating(
          &MultideviceHandler::NotifySmartLockSignInEnabledChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      multidevice_setup::kSmartLockSigninAllowedPrefName,
      base::BindRepeating(
          &MultideviceHandler::NotifySmartLockSignInAllowedChanged,
          base::Unretained(this)));
}

void MultideviceHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.RemoveAll();

  if (multidevice_setup_client_)
    multidevice_setup_observer_.Remove(multidevice_setup_client_);

  if (android_sms_pairing_state_tracker_) {
    android_sms_pairing_state_tracker_observer_.Remove(
        android_sms_pairing_state_tracker_);
  }

  if (android_sms_app_manager_)
    android_sms_app_manager_observer_.Remove(android_sms_app_manager_);

  // Ensure that pending callbacks do not complete and cause JS to be evaluated.
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

void MultideviceHandler::OnHostStatusChanged(
    const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
        host_status_with_device) {
  UpdatePageContent();
  NotifyAndroidSmsInfoChange();
}

void MultideviceHandler::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  UpdatePageContent();
  NotifyAndroidSmsInfoChange();
}

void MultideviceHandler::OnPairingStateChanged() {
  UpdatePageContent();
  NotifyAndroidSmsInfoChange();
}

void MultideviceHandler::OnInstalledAppUrlChanged() {
  UpdatePageContent();
  NotifyAndroidSmsInfoChange();
}

void MultideviceHandler::UpdatePageContent() {
  std::unique_ptr<base::DictionaryValue> page_content_dictionary =
      GeneratePageContentDataDictionary();
  DCHECK(page_content_dictionary);
  PA_LOG(VERBOSE) << "Updating MultiDevice settings page content with: "
                  << *page_content_dictionary << ".";
  FireWebUIListener("settings.updateMultidevicePageContentData",
                    *page_content_dictionary);
}

void MultideviceHandler::NotifyAndroidSmsInfoChange() {
  auto android_sms_info = GenerateAndroidSmsInfo();
  FireWebUIListener("settings.onAndroidSmsInfoChange", *android_sms_info);
}

void MultideviceHandler::HandleShowMultiDeviceSetupDialog(
    const base::ListValue* args) {
  DCHECK(args->empty());
  multidevice_setup::MultiDeviceSetupDialog::Show();
}

void MultideviceHandler::HandleGetPageContent(const base::ListValue* args) {
  // This callback is expected to be the first one executed when the page is
  // loaded, so it should be the one to allow JS calls.
  AllowJavascript();

  std::string callback_id;
  bool result = args->GetString(0, &callback_id);
  DCHECK(result);

  std::unique_ptr<base::DictionaryValue> page_content_dictionary =
      GeneratePageContentDataDictionary();
  DCHECK(page_content_dictionary);
  PA_LOG(VERBOSE) << "Responding to getPageContentData() request with: "
                  << *page_content_dictionary << ".";

  ResolveJavascriptCallback(base::Value(callback_id), *page_content_dictionary);
}

void MultideviceHandler::HandleSetFeatureEnabledState(
    const base::ListValue* args) {
  std::string callback_id;
  bool result = args->GetString(0, &callback_id);
  DCHECK(result);

  int feature_as_int;
  result = args->GetInteger(1, &feature_as_int);
  DCHECK(result);

  auto feature = static_cast<multidevice_setup::mojom::Feature>(feature_as_int);
  DCHECK(multidevice_setup::mojom::IsKnownEnumValue(feature));

  bool enabled;
  result = args->GetBoolean(2, &enabled);
  DCHECK(result);

  base::Optional<std::string> auth_token;
  std::string possible_token_value;
  if (args->GetString(3, &possible_token_value))
    auth_token = possible_token_value;

  multidevice_setup_client_->SetFeatureEnabledState(
      feature, enabled, auth_token,
      base::BindOnce(&MultideviceHandler::OnSetFeatureStateEnabledResult,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void MultideviceHandler::HandleRemoveHostDevice(const base::ListValue* args) {
  DCHECK(args->empty());
  multidevice_setup_client_->RemoveHostDevice();
}

void MultideviceHandler::HandleRetryPendingHostSetup(
    const base::ListValue* args) {
  DCHECK(args->empty());
  multidevice_setup_client_->RetrySetHostNow(
      base::BindOnce(&OnRetrySetHostNowResult));
}

void MultideviceHandler::HandleSetUpAndroidSms(const base::ListValue* args) {
  DCHECK(args->empty());
  android_sms_app_manager_->SetUpAndLaunchAndroidSmsApp();
}

void MultideviceHandler::HandleGetSmartLockSignInEnabled(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  bool signInEnabled = prefs_->GetBoolean(
      proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled);
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(signInEnabled));
}

void MultideviceHandler::HandleSetSmartLockSignInEnabled(
    const base::ListValue* args) {
  bool enabled = false;
  CHECK(args->GetBoolean(0, &enabled));

  std::string auth_token;
  bool auth_token_present = args->GetString(1, &auth_token);

  // Either the user is disabling sign-in, or they are enabling it and the auth
  // token must be present.
  CHECK(!enabled || auth_token_present);

  // Only check auth token if the user is attempting to enable sign-in.
  if (enabled && !IsAuthTokenValid(auth_token))
    return;

  prefs_->SetBoolean(
      proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled, enabled);
}

void MultideviceHandler::HandleGetSmartLockSignInAllowed(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  bool sign_in_allowed =
      prefs_->GetBoolean(multidevice_setup::kSmartLockSigninAllowedPrefName);
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(sign_in_allowed));
}

std::unique_ptr<base::DictionaryValue>
MultideviceHandler::GenerateAndroidSmsInfo() {
  base::Optional<GURL> app_url;
  if (android_sms_app_manager_)
    app_url = android_sms_app_manager_->GetCurrentAppUrl();
  if (!app_url)
    app_url = android_sms::GetAndroidMessagesURL();

  auto android_sms_info = std::make_unique<base::DictionaryValue>();
  android_sms_info->SetString(
      kAndroidSmsInfoOriginKey,
      ContentSettingsPattern::FromURLNoWildcard(*app_url).ToString());

  chromeos::multidevice_setup::mojom::FeatureState messages_state =
      multidevice_setup_client_->GetFeatureState(
          chromeos::multidevice_setup::mojom::Feature::kMessages);
  bool enabled_state =
      messages_state ==
          chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser ||
      messages_state == chromeos::multidevice_setup::mojom::FeatureState::
                            kFurtherSetupRequired;
  android_sms_info->SetBoolean(kAndroidSmsInfoEnabledKey, enabled_state);

  return android_sms_info;
}

void MultideviceHandler::HandleGetAndroidSmsInfo(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(*callback_id, *GenerateAndroidSmsInfo());
}

void MultideviceHandler::OnSetFeatureStateEnabledResult(
    const std::string& js_callback_id,
    bool success) {
  ResolveJavascriptCallback(base::Value(js_callback_id), base::Value(success));
}

std::unique_ptr<base::DictionaryValue>
MultideviceHandler::GeneratePageContentDataDictionary() {
  auto page_content_dictionary = std::make_unique<base::DictionaryValue>();

  multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
      host_status_with_device = GetHostStatusWithDevice();
  multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap feature_states =
      GetFeatureStatesMap();

  page_content_dictionary->SetInteger(
      kPageContentDataModeKey,
      static_cast<int32_t>(host_status_with_device.first));
  page_content_dictionary->SetInteger(
      kPageContentDataBetterTogetherStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kBetterTogetherSuite]));
  page_content_dictionary->SetInteger(
      kPageContentDataInstantTetheringStateKey,
      static_cast<int32_t>(
          feature_states
              [multidevice_setup::mojom::Feature::kInstantTethering]));
  page_content_dictionary->SetInteger(
      kPageContentDataMessagesStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kMessages]));
  page_content_dictionary->SetInteger(
      kPageContentDataSmartLockStateKey,
      static_cast<int32_t>(
          feature_states[multidevice_setup::mojom::Feature::kSmartLock]));

  if (host_status_with_device.second) {
    page_content_dictionary->SetString(kPageContentDataHostDeviceNameKey,
                                       host_status_with_device.second->name());
  }

  page_content_dictionary->SetBoolean(
      kIsAndroidSmsPairingComplete,
      android_sms_pairing_state_tracker_
          ? android_sms_pairing_state_tracker_->IsAndroidSmsPairingComplete()
          : false);

  return page_content_dictionary;
}

void MultideviceHandler::NotifySmartLockSignInEnabledChanged() {
  bool sign_in_enabled = prefs_->GetBoolean(
      proximity_auth::prefs::kProximityAuthIsChromeOSLoginEnabled);
  FireWebUIListener("smart-lock-signin-enabled-changed",
                    base::Value(sign_in_enabled));
}

void MultideviceHandler::NotifySmartLockSignInAllowedChanged() {
  bool sign_in_allowed =
      prefs_->GetBoolean(multidevice_setup::kSmartLockSigninAllowedPrefName);
  FireWebUIListener("smart-lock-signin-allowed-changed",
                    base::Value(sign_in_allowed));
}

bool MultideviceHandler::IsAuthTokenValid(const std::string& auth_token) {
  Profile* profile = Profile::FromWebUI(web_ui());
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      chromeos::quick_unlock::QuickUnlockFactory::GetForProfile(profile);
  return quick_unlock_storage->GetAuthToken() &&
         auth_token == quick_unlock_storage->GetAuthToken()->Identifier();
}

multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice
MultideviceHandler::GetHostStatusWithDevice() {
  if (multidevice_setup_client_)
    return multidevice_setup_client_->GetHostStatus();

  return multidevice_setup::MultiDeviceSetupClient::
      GenerateDefaultHostStatusWithDevice();
}

multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap
MultideviceHandler::GetFeatureStatesMap() {
  if (multidevice_setup_client_)
    return multidevice_setup_client_->GetFeatureStates();

  return multidevice_setup::MultiDeviceSetupClient::
      GenerateDefaultFeatureStatesMap();
}

}  // namespace settings

}  // namespace chromeos
