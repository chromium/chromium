// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/credential_manager_impl.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/credential_manager_logger.h"
#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"
#include "components/password_manager/core/browser/credential_manager_utils.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace password_manager {

using password_manager_util::GetLoginMatchType;
using password_manager_util::GetMatchType;

namespace {

void RunGetCallback(GetCallback callback, const CredentialInfo& info) {
  std::move(callback).Run(CredentialManagerError::SUCCESS, info);
}

}  // namespace

CredentialManagerImpl::CredentialManagerImpl(PasswordManagerClient* client)
    : client_(client), leak_delegate_(client) {}

CredentialManagerImpl::~CredentialManagerImpl() = default;

void CredentialManagerImpl::Store(const CredentialInfo& credential,
                                  StoreCallback callback) {
  const url::Origin origin = GetOrigin();
  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogStoreCredential(origin, credential.type);
  }

  // Send acknowledge response back.
  std::move(callback).Run();

  if (credential.type == CredentialType::CREDENTIAL_TYPE_EMPTY ||
      !client_->IsSavingAndFillingEnabled(origin.GetURL())) {
    return;
  }

  client_->NotifyStorePasswordCalled();

  std::unique_ptr<PasswordForm> form(
      CreatePasswordFormFromCredentialInfo(credential, origin));

  // Check whether a stored password credential was leaked.
  if (credential.type == CredentialType::CREDENTIAL_TYPE_PASSWORD) {
    leak_delegate_.StartLeakCheck(LeakDetectionInitiator::kSignInCheck, *form,
                                  origin.GetURL());
  }

  std::string signon_realm = origin.GetURL().spec();
  PasswordFormDigest observed_digest(PasswordForm::Scheme::kHtml, signon_realm,
                                     origin.GetURL());

  // Create a custom form fetcher without HTTP->HTTPS migration as the API is
  // only available on HTTPS origins.
  auto form_fetcher = std::make_unique<FormFetcherImpl>(
      observed_digest, client_, /*should_migrate_http_passwords=*/false);
  form_manager_ = std::make_unique<CredentialManagerPasswordFormManager>(
      client_, std::move(form), this, nullptr, std::move(form_fetcher));
}

void CredentialManagerImpl::PreventSilentAccess(
    PreventSilentAccessCallback callback) {
  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogPreventSilentAccess(GetOrigin());
  }
  // Send acknowledge response back.
  std::move(callback).Run();

  PasswordStoreInterface* store = GetProfilePasswordStore();
  if (!store || !client_->IsSavingAndFillingEnabled(GetOrigin().GetURL())) {
    return;
  }

  if (!pending_require_user_mediation_) {
    pending_require_user_mediation_ =
        std::make_unique<CredentialManagerPendingPreventSilentAccessTask>(this);
  }
  pending_require_user_mediation_->AddOrigin(GetSynthesizedFormForOrigin());
}

void CredentialManagerImpl::Get(CredentialMediationRequirement mediation,
                                int requested_credential_type_flags,
                                const std::vector<GURL>& federations,
                                GetCallback callback) {
  using metrics_util::LogCredentialManagerGetResult;

  PasswordStoreInterface* store = GetProfilePasswordStore();
  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogRequestCredential(GetOrigin(), mediation, federations);
  }
  if (pending_request_ || !store) {
    // Callback error.
    std::move(callback).Run(
        pending_request_ ? CredentialManagerError::PENDING_REQUEST
                         : CredentialManagerError::PASSWORDSTOREUNAVAILABLE,
        std::nullopt);
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kRejected, mediation);
    return;
  }

  // Return an empty credential if the current page has TLS errors, or if the
  // page is being prerendered.
  if (!client_->IsFillingEnabled(GetOrigin().GetURL())) {
    std::move(callback).Run(CredentialManagerError::SUCCESS, CredentialInfo());
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNone, mediation);
    return;
  }
  // Return an empty credential for incognito mode.
  if (client_->IsOffTheRecord()) {
    // Callback with empty credential info.
    std::move(callback).Run(CredentialManagerError::SUCCESS, CredentialInfo());
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNoneIncognito, mediation);
    return;
  }
  // Return an empty credential if zero-click is required but disabled.
  if (mediation == CredentialMediationRequirement::kSilent &&
      !IsZeroClickAllowed()) {
    // Callback with empty credential info.
    std::move(callback).Run(CredentialManagerError::SUCCESS, CredentialInfo());
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNoneZeroClickOff, mediation);
    return;
  }
  pending_request_ = std::make_unique<CredentialManagerPendingRequestTask>(
      this, base::BindOnce(&RunGetCallback, std::move(callback)), mediation,
      requested_credential_type_flags, federations,
      GetSynthesizedFormForOrigin());
}

void CredentialManagerImpl::ResetPendingRequest() {
  pending_request_.reset();
}

bool CredentialManagerImpl::IsZeroClickAllowed() const {
  return client_->IsAutoSignInEnabled() && !client_->IsOffTheRecord();
}

PasswordFormDigest CredentialManagerImpl::GetSynthesizedFormForOrigin() const {
  PasswordFormDigest digest = {PasswordForm::Scheme::kHtml, std::string(),
                               GetOrigin().GetURL()};
  digest.signon_realm = digest.url.spec();
  return digest;
}

url::Origin CredentialManagerImpl::GetOrigin() const {
  return client_->GetLastCommittedOrigin();
}

void CredentialManagerImpl::SendCredential(SendCredentialCallback send_callback,
                                           const CredentialInfo& info) {
  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogSendCredential(GetOrigin(), info.type);
  }
  std::move(send_callback).Run(info);
  pending_request_.reset();
}

void CredentialManagerImpl::SendPasswordForm(
    SendCredentialCallback send_callback,
    CredentialMediationRequirement mediation,
    const PasswordForm* form) {
  CredentialInfo info;
  if (form) {
    info = PasswordFormToCredentialInfo(*form);
    PasswordStoreInterface* store = form->IsUsingAccountStore()
                                        ? GetAccountPasswordStore()
                                        : GetProfilePasswordStore();
    if (store) {
      if (form->skip_zero_click && IsZeroClickAllowed()) {
        PasswordForm update_form = *form;
        update_form.skip_zero_click = false;
        store->UpdateLogin(update_form);
      }
    }
    base::RecordAction(
        base::UserMetricsAction("CredentialManager_AccountChooser_Accepted"));
    metrics_util::LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kAccountChooser, mediation);
  } else {
    base::RecordAction(
        base::UserMetricsAction("CredentialManager_AccountChooser_Dismissed"));
    metrics_util::LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNone, mediation);
  }
  SendCredential(std::move(send_callback), info);
}

PasswordManagerClient* CredentialManagerImpl::client() const {
  return client_;
}

PasswordStoreInterface* CredentialManagerImpl::GetProfilePasswordStore() {
  return client_ ? client_->GetProfilePasswordStore() : nullptr;
}

PasswordStoreInterface* CredentialManagerImpl::GetAccountPasswordStore() {
  return client_ ? client_->GetAccountPasswordStore() : nullptr;
}

void CredentialManagerImpl::DoneRequiringUserMediation() {
  DCHECK(pending_require_user_mediation_);
  pending_require_user_mediation_.reset();
}

void CredentialManagerImpl::OnProvisionalSaveComplete() {
  DCHECK(form_manager_);
  const PasswordForm& form = form_manager_->GetPendingCredentials();
  DCHECK(client_->IsSavingAndFillingEnabled(form.url));

  if (form.match_type.has_value()) {
    // Having PSL or affiliated web match implies there is no credential with an
    // exactly matching origin and username. In order to avoid showing a save
    // bubble to the user Save() is called directly. Save prompt is still
    // offered for grouped credentials.
    GetLoginMatchType match_type = GetMatchType(form);
    if (match_type == GetLoginMatchType::kPSL ||
        (match_type == GetLoginMatchType::kAffiliated &&
         !affiliations::IsValidAndroidFacetURI(form.signon_realm))) {
      form_manager_->Save();
      return;
    }
  }
  if (form.federation_origin.IsValid()) {
    // If this is a federated credential, check it against the federated matches
    // produced by the PasswordFormManager. If a match is found, update it and
    // return.
    for (const password_manager::PasswordForm& match :
         form_manager_->GetFormFetcher()->GetFederatedMatches()) {
      if (match.username_value == form.username_value &&
          match.federation_origin == form.federation_origin) {
        form_manager_->Save();
        return;
      }
    }
  } else if (!form_manager_->IsNewLogin()) {
    // Otherwise, if this is not a new password credential, update the existing
    // credential prompting confirmation helium bubble to the user. This will
    // also update the 'skip_zero_click' state, as we've gotten an explicit
    // signal that the page understands the credential management API and so can
    // be trusted to notify us when they sign the user out.
    bool is_update_confirmation = form_manager_->IsPasswordUpdate();
    form_manager_->Save();
    if (is_update_confirmation) {
      client_->AutomaticPasswordSave(std::move(form_manager_),
                                     /*is_update_confirmation=*/true);
    }
    return;
  }

  // Otherwise, this is a new form, so as the user if they'd like to save.
  client_->PromptUserToSaveOrUpdatePassword(std::move(form_manager_), false);
}

}  // namespace password_manager
