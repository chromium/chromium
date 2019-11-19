// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/credential_manager_impl.h"

#include <string>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "components/password_manager/core/browser/credential_manager_logger.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace password_manager {

namespace {

void RunGetCallback(GetCallback callback, const CredentialInfo& info) {
  std::move(callback).Run(CredentialManagerError::SUCCESS, info);
}

}  // namespace

CredentialManagerImpl::CredentialManagerImpl(PasswordManagerClient* client)
    : client_(client), leak_delegate_(client) {
  auto_signin_enabled_.Init(prefs::kCredentialsEnableAutosignin,
                            client_->GetPrefs());
}

CredentialManagerImpl::~CredentialManagerImpl() {}

void CredentialManagerImpl::Store(const CredentialInfo& credential,
                                  StoreCallback callback) {
  DCHECK_NE(CredentialType::CREDENTIAL_TYPE_EMPTY, credential.type);

  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogStoreCredential(GetLastCommittedURL(), credential.type);
  }

  // Send acknowledge response back.
  std::move(callback).Run();

  const GURL origin = GetLastCommittedURL();
  if (!client_->IsSavingAndFillingEnabled(origin) ||
      !client_->OnCredentialManagerUsed())
    return;

  client_->NotifyStorePasswordCalled();

  std::unique_ptr<autofill::PasswordForm> form(
      CreatePasswordFormFromCredentialInfo(credential, origin));

  // Check whether a stored password credential was leaked.
  if (credential.type == CredentialType::CREDENTIAL_TYPE_PASSWORD)
    leak_delegate_.StartLeakCheck(*form);

  std::string signon_realm = origin.GetOrigin().spec();
  PasswordStore::FormDigest observed_digest(
      autofill::PasswordForm::Scheme::kHtml, signon_realm, origin);

  // Create a custom form fetcher without HTTP->HTTPS migration as the API is
  // only available on HTTPS origins.
  auto form_fetcher =
      std::make_unique<FormFetcherImpl>(observed_digest, client_, false);
  form_manager_ = std::make_unique<CredentialManagerPasswordFormManager>(
      client_, std::move(form), this, nullptr, std::move(form_fetcher));
}

void CredentialManagerImpl::PreventSilentAccess(
    PreventSilentAccessCallback callback) {
  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogPreventSilentAccess(GetLastCommittedURL());
  }
  // Send acknowledge response back.
  std::move(callback).Run();

  PasswordStore* store = GetPasswordStore();
  if (!store || !client_->IsSavingAndFillingEnabled(GetLastCommittedURL()) ||
      !client_->OnCredentialManagerUsed())
    return;

  if (!pending_require_user_mediation_) {
    pending_require_user_mediation_.reset(
        new CredentialManagerPendingPreventSilentAccessTask(this));
  }
  pending_require_user_mediation_->AddOrigin(GetSynthesizedFormForOrigin());
}

void CredentialManagerImpl::Get(CredentialMediationRequirement mediation,
                                bool include_passwords,
                                const std::vector<GURL>& federations,
                                GetCallback callback) {
  using metrics_util::LogCredentialManagerGetResult;

  PasswordStore* store = GetPasswordStore();
  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogRequestCredential(GetLastCommittedURL(), mediation, federations);
  }
  if (pending_request_ || !store) {
    // Callback error.
    std::move(callback).Run(
        pending_request_ ? CredentialManagerError::PENDING_REQUEST
                         : CredentialManagerError::PASSWORDSTOREUNAVAILABLE,
        base::nullopt);
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kRejected, mediation);
    return;
  }

  // Return an empty credential if the current page has TLS errors, or if the
  // page is being prerendered.
  if (!client_->IsFillingEnabled(GetLastCommittedURL()) ||
      !client_->OnCredentialManagerUsed()) {
    std::move(callback).Run(CredentialManagerError::SUCCESS, CredentialInfo());
    LogCredentialManagerGetResult(
        metrics_util::CredentialManagerGetResult::kNone, mediation);
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

  pending_request_.reset(new CredentialManagerPendingRequestTask(
      this, base::Bind(&RunGetCallback, base::Passed(&callback)), mediation,
      include_passwords, federations));
  // This will result in a callback to
  // PendingRequestTask::OnGetPasswordStoreResults().
  GetPasswordStore()->GetLogins(GetSynthesizedFormForOrigin(),
                                pending_request_.get());
}

bool CredentialManagerImpl::IsZeroClickAllowed() const {
  return *auto_signin_enabled_ && !client_->IsIncognito();
}

PasswordStore::FormDigest CredentialManagerImpl::GetSynthesizedFormForOrigin()
    const {
  PasswordStore::FormDigest digest = {autofill::PasswordForm::Scheme::kHtml,
                                      std::string(),
                                      GetLastCommittedURL().GetOrigin()};
  digest.signon_realm = digest.origin.spec();
  return digest;
}

GURL CredentialManagerImpl::GetOrigin() const {
  return GetLastCommittedURL().GetOrigin();
}

void CredentialManagerImpl::SendCredential(
    const SendCredentialCallback& send_callback,
    const CredentialInfo& info) {
  DCHECK(pending_request_);
  DCHECK(send_callback == pending_request_->send_callback());

  if (password_manager_util::IsLoggingActive(client_)) {
    CredentialManagerLogger(client_->GetLogManager())
        .LogSendCredential(GetLastCommittedURL(), info.type);
  }
  send_callback.Run(info);
  pending_request_.reset();
}

void CredentialManagerImpl::SendPasswordForm(
    const SendCredentialCallback& send_callback,
    CredentialMediationRequirement mediation,
    const autofill::PasswordForm* form) {
  CredentialInfo info;
  if (form) {
    password_manager::CredentialType type_to_return =
        form->federation_origin.opaque()
            ? CredentialType::CREDENTIAL_TYPE_PASSWORD
            : CredentialType::CREDENTIAL_TYPE_FEDERATED;
    info = CredentialInfo(*form, type_to_return);
    if (PasswordStore* store = GetPasswordStore()) {
      if (form->skip_zero_click && IsZeroClickAllowed()) {
        autofill::PasswordForm update_form = *form;
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
  SendCredential(send_callback, info);
}

PasswordManagerClient* CredentialManagerImpl::client() const {
  return client_;
}

PasswordStore* CredentialManagerImpl::GetPasswordStore() {
  return client_ ? client_->GetProfilePasswordStore() : nullptr;
}

void CredentialManagerImpl::DoneRequiringUserMediation() {
  DCHECK(pending_require_user_mediation_);
  pending_require_user_mediation_.reset();
}

void CredentialManagerImpl::OnProvisionalSaveComplete() {
  DCHECK(form_manager_);
  const autofill::PasswordForm& form = form_manager_->GetPendingCredentials();
  DCHECK(client_->IsSavingAndFillingEnabled(form.origin));

  if (form_manager_->IsPendingCredentialsPublicSuffixMatch()) {
    // Having a credential with a PSL match implies there is no credential with
    // an exactly matching origin and username. In order to avoid showing a save
    // bubble to the user Save() is called directly.
    form_manager_->Save();
    return;
  }

  if (!form.federation_origin.opaque()) {
    // If this is a federated credential, check it against the federated matches
    // produced by the PasswordFormManager. If a match is found, update it and
    // return.
    for (auto* match : form_manager_->GetFormFetcher()->GetFederatedMatches()) {
      if (match->username_value == form.username_value &&
          match->federation_origin.IsSameOriginWith(form.federation_origin)) {
        form_manager_->Update(*match);
        return;
      }
    }
  } else if (!form_manager_->IsNewLogin()) {
    // Otherwise, if this is not a new password credential, update the existing
    // credential without prompting the user. This will also update the
    // 'skip_zero_click' state, as we've gotten an explicit signal that the page
    // understands the credential management API and so can be trusted to notify
    // us when they sign the user out.
    form_manager_->Update(form_manager_->GetPendingCredentials());
    return;
  }

  // Otherwise, this is a new form, so as the user if they'd like to save.
  client_->PromptUserToSaveOrUpdatePassword(std::move(form_manager_), false);
}

GURL CredentialManagerImpl::GetLastCommittedURL() const {
  return client_->GetLastCommittedEntryURL();
}

}  // namespace password_manager
