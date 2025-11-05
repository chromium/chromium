// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_client.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/version_info/channel.h"
#include "url/origin.h"

namespace password_manager {

bool PasswordManagerClient::IsSavingAndFillingEnabled(const GURL& url) const {
  return true;
}

bool PasswordManagerClient::IsFillingEnabled(const GURL& url) const {
  return true;
}

bool PasswordManagerClient::IsFieldFilledWithOtp(
    autofill::FormGlobalId form_id,
    autofill::FieldGlobalId field_id) {
  return false;
}

bool PasswordManagerClient::IsAutoSignInEnabled() const {
  return false;
}

#if BUILDFLAG(IS_ANDROID)
void PasswordManagerClient::ShowPasswordManagerErrorMessage(
    ErrorMessageFlowType flow_type,
    password_manager::PasswordStoreBackendErrorType error_type) {}

void PasswordManagerClient::ShowKeyboardReplacingSurface(
    PasswordManagerDriver* driver,
    const autofill::PasswordSuggestionRequest& request) {}
#endif

bool PasswordManagerClient::IsReauthBeforeFillingRequired(
    device_reauth::DeviceAuthenticator*) {
  return false;
}

std::unique_ptr<device_reauth::DeviceAuthenticator>
PasswordManagerClient::GetDeviceAuthenticator() {
  return nullptr;
}

void PasswordManagerClient::GeneratePassword(
    autofill::password_generation::PasswordGenerationType type) {}

void PasswordManagerClient::UpdateCredentialCache(
    const url::Origin& origin,
    base::span<const PasswordForm> best_matches,
    bool is_blocklisted,
    std::optional<PasswordStoreBackendError> backend_error) {}

void PasswordManagerClient::PasswordWasAutofilled(
    base::span<const PasswordForm> best_matches,
    const url::Origin& origin,
    base::span<const PasswordForm> federated_matches,
    bool was_autofilled_on_pageload) {}

void PasswordManagerClient::AutofillHttpAuth(
    const PasswordForm& preferred_match,
    const PasswordFormManagerForUI* form_manager) {}

void PasswordManagerClient::NotifyUserCredentialsWereLeaked(
    LeakedPasswordDetails details) {}

bool PasswordManagerClient::WasLastNavigationHTTPError() const {
  return false;
}

net::CertStatus PasswordManagerClient::GetMainFrameCertStatus() const {
  return 0;
}

void PasswordManagerClient::PromptUserToEnableAutosignin() {}

bool PasswordManagerClient::IsOffTheRecord() const {
  return false;
}

password_manager::LeakDetectionInitiator
PasswordManagerClient::GetLeakDetectionInitiator() {
  return password_manager::LeakDetectionInitiator::kSignInCheck;
}

profile_metrics::BrowserProfileType PasswordManagerClient::GetProfileType()
    const {
  // This is an abstract interface and thus never instantiated directly,
  // therefore it is safe to always return |kRegular| here.
  return profile_metrics::BrowserProfileType::kRegular;
}

const PasswordManagerInterface* PasswordManagerClient::GetPasswordManager()
    const {
  return nullptr;
}

PasswordManagerInterface* PasswordManagerClient::GetPasswordManager() {
  return const_cast<PasswordManagerInterface*>(
      static_cast<const PasswordManagerClient*>(this)->GetPasswordManager());
}

const PasswordFeatureManager* PasswordManagerClient::GetPasswordFeatureManager()
    const {
  return nullptr;
}

PasswordFeatureManager* PasswordManagerClient::GetPasswordFeatureManager() {
  return const_cast<PasswordFeatureManager*>(
      static_cast<const PasswordManagerClient*>(this)
          ->GetPasswordFeatureManager());
}

HttpAuthManager* PasswordManagerClient::GetHttpAuthManager() {
  return nullptr;
}

OtpManager* PasswordManagerClient::GetOtpManager() {
  return nullptr;
}

autofill::AutofillCrowdsourcingManager*
PasswordManagerClient::GetAutofillCrowdsourcingManager() {
  return nullptr;
}

bool PasswordManagerClient::IsCommittedMainFrameSecure() const {
  return false;
}

autofill::LogManager* PasswordManagerClient::GetCurrentLogManager() {
  return nullptr;
}

void PasswordManagerClient::AnnotateNavigationEntry(bool has_password_field) {}

autofill::LanguageCode PasswordManagerClient::GetPageLanguage() const {
  return autofill::LanguageCode();
}

PasswordRequirementsService*
PasswordManagerClient::GetPasswordRequirementsService() {
  // Not impemented but that is a valid state as per interface definition.
  // Therefore, don't call NOTIMPLEMENTED() here.
  return nullptr;
}

favicon::FaviconService* PasswordManagerClient::GetFaviconService() {
  return nullptr;
}

password_manager::FieldInfoManager* PasswordManagerClient::GetFieldInfoManager()
    const {
  return nullptr;
}

network::mojom::NetworkContext* PasswordManagerClient::GetNetworkContext()
    const {
  return nullptr;
}

WebAuthnCredentialsDelegate*
PasswordManagerClient::GetWebAuthnCredentialsDelegateForDriver(
    PasswordManagerDriver* driver) {
  return nullptr;
}

void PasswordManagerClient::TriggerUserPerceptionOfPasswordManagerSurvey(
    const std::string& filling_assistance) {}

#if BUILDFLAG(IS_ANDROID)
webauthn::WebAuthnCredManDelegate*
PasswordManagerClient::GetWebAuthnCredManDelegateForDriver(
    PasswordManagerDriver* driver) {
  return nullptr;
}

void PasswordManagerClient::MarkSharedCredentialsAsNotified(const GURL& url) {}

#endif  // BUILDFLAG(IS_ANDROID)

version_info::Channel PasswordManagerClient::GetChannel() const {
  return version_info::Channel::UNKNOWN;
}

void PasswordManagerClient::RefreshPasswordManagerSettingsIfNeeded() const {
  // For most implementations settings do not need to be refreshed.
}

void PasswordManagerClient::TriggerSignIn(
    signin_metrics::AccessPoint access_point) const {}

UndoPasswordChangeController*
PasswordManagerClient::GetUndoPasswordChangeController() {
  return nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
bool PasswordManagerClient::IsActorTaskActive() {
  return false;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
