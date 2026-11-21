// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm/gpm_browser_authenticator_controller.h"

#include <algorithm>
#include <iterator>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/gpm/local_gpm_passkeys_util.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_types.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

using Step = AuthenticatorRequestDialogModel::Step;

// Maximum time to wait for security domain state before falling back to the
// mechanism selection sheet when GPM is selected while still loading.
constexpr base::TimeDelta kLoadingTimeout = base::Milliseconds(500);

void ResetDeclinedBootstrappingCount(Profile* profile) {
  profile->GetPrefs()->SetInteger(
      webauthn::pref_names::kEnclaveDeclinedGPMBootstrappingCount, 0);
  profile->GetPrefs()->SetInteger(
      webauthn::pref_names::kEnclaveDeclinedGPMCredentialCreationCount, 0);
}

}  // namespace

GpmBrowserAuthenticatorController::GpmBrowserAuthenticatorController(
    content::RenderFrameHost& render_frame_host,
    AuthenticatorRequestDialogModel& model,
    const std::string& rp_id,
    device::FidoRequestType request_type,
    device::UserVerificationRequirement user_verification_requirement,
    RequestCallback request_callback)
    : render_frame_host_id_(render_frame_host.GetGlobalId()),
      rp_id_(rp_id),
      request_type_(request_type),
      model_(model),
      request_callback_(std::move(request_callback)),
      loading_timeout_(
          GpmTickAndTaskRunnerProvider::GetTickClock(&render_frame_host)) {
  CHECK(webauthn::IsLocalGpmPasskeysEnabled());
  if (scoped_refptr<base::SequencedTaskRunner> task_runner =
          GpmTickAndTaskRunnerProvider::GetTaskRunner(&render_frame_host)) {
    loading_timeout_.SetTaskRunner(std::move(task_runner));
  }
  model_observer_.Observe(&*model_);

  Profile* const profile = GetProfile();
  // `TrustedVaultServiceFactory` returns null by default in `TestingProfile`
  // (`ServiceIsNULLWhileTesting()`).
  if (trusted_vault::TrustedVaultService* trusted_vault_service =
          TrustedVaultServiceFactory::GetForProfile(profile)) {
    trusted_vault_client_ = trusted_vault_service->GetTrustedVaultClient(
        trusted_vault::SecurityDomainId::kPasskeys);
    CHECK(trusted_vault_client_);
    trusted_vault_client_observer_.Observe(trusted_vault_client_);
  }

  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(profile);
  CHECK(passkey_model);
  creds_ = passkey_model->GetPasskeys(
      rp_id_, webauthn::PasskeyModel::ShadowedCredentials::kExclude);
  std::erase_if(creds_, [](const auto& cred) { return cred.hidden(); });

  if (!trusted_vault_client_ ||
      (creds_.empty() &&
       request_type == device::FidoRequestType::kGetAssertion)) {
    FIDO_LOG(EVENT) << "GPM passkeys disabled";
    SetAccountState(AccountState::kNone);
    SetActive(EnclaveEnabledStatus::kDisabled);
    return;
  }

  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  GoogleServiceAuthError signin_error =
      identity_manager->GetErrorStateOfRefreshTokenForAccount(
          account.account_id);
  if (signin_error.IsPersistentError()) {
    FIDO_LOG(EVENT) << "Recoverable sign-in error: " << signin_error.ToString();
    // TODO(crbug.com/570170982): Record a browser-authenticator event metric.
    SetAccountState(AccountState::kNone);
    SetActive(EnclaveEnabledStatus::kEnabledAndReauthNeeded);
    return;
  }

  SetActive(EnclaveEnabledStatus::kEnabled);

  trusted_vault_client_->FetchKeys(
      account, base::BindOnce(&GpmBrowserAuthenticatorController::OnKeysFetched,
                              weak_ptr_factory_.GetWeakPtr(), account));
}

GpmBrowserAuthenticatorController::~GpmBrowserAuthenticatorController() =
    default;

std::optional<EnclaveUserVerificationMethod>
GpmBrowserAuthenticatorController::GetUserVerificationMethod() const {
  if (account_state_ != AccountState::kReady) {
    return std::nullopt;
  }
  // TODO(crbug.com/570170982): Support local OS user verification and GPM PIN
  // verification/creation.
  return EnclaveUserVerificationMethod::kUserPresenceOnly;
}

bool GpmBrowserAuthenticatorController::is_active() const {
  return *is_active_;
}

bool GpmBrowserAuthenticatorController::ready_for_ui() const {
  return ready_for_ui_;
}

const std::vector<sync_pb::WebauthnCredentialSpecifics>&
GpmBrowserAuthenticatorController::creds() const {
  return creds_;
}

GpmBrowserAuthenticatorController::AccountReadyState
GpmBrowserAuthenticatorController::account_ready_state() const {
  switch (account_state_) {
    case AccountState::kLoading:
      return AccountReadyState::kLoading;
    case AccountState::kReady:
      return AccountReadyState::kReady;
    case AccountState::kNone:
    case AccountState::kRecoverable:
    case AccountState::kIrrecoverable:
    case AccountState::kEmpty:
      return AccountReadyState::kNotReady;
  }
}

void GpmBrowserAuthenticatorController::RunWhenAccountReady(
    base::OnceClosure callback) {
  if (account_state_ != AccountState::kLoading) {
    std::move(callback).Run();
    return;
  }

  CHECK(!waiting_for_account_state_);
  waiting_for_account_state_ = std::move(callback);
}

bool GpmBrowserAuthenticatorController::OnAuthenticatorError() {
  model_->SetStep(Step::kGPMError);
  return true;
}

void GpmBrowserAuthenticatorController::OnGPMCreationSelected() {
  if (transaction_started_) {
    return;
  }
  // Reset after each GPM selection to ensure correct metric emission.
  model_->in_onboarding_flow = false;

  if (account_state_ != AccountState::kLoading) {
    RecordGPMMakeCredentialEvent(
        webauthn::metrics::GPMMakeCredentialEvents::kStarted);
  }

  switch (account_state_) {
    case AccountState::kReady:
      model_->SetStep(Step::kGPMCreatePasskey);
      break;

    case AccountState::kRecoverable:
    case AccountState::kIrrecoverable:
      // TODO(crbug.com/570170982): Record a browser-authenticator event metric.
      model_->SetStep(Step::kGPMTrustThisComputerCreation);
      break;

    case AccountState::kLoading:
      waiting_for_account_state_ = base::BindOnce(
          &GpmBrowserAuthenticatorController::OnGPMCreationSelected,
          weak_ptr_factory_.GetWeakPtr());
      OnGpmSelectedWhileLoading();
      break;

    case AccountState::kEmpty:
      // TODO(crbug.com/570170982): Enter the GPM onboarding flow
      // (`Step::kGPMCreatePasskey` -> `Step::kGPMCreatePin`) once GpmPinService
      // is wired up for PIN creation.
      NOTIMPLEMENTED();
      break;
    case AccountState::kNone:
      model_->SetStep(Step::kGPMError);
      break;
  }
}

void GpmBrowserAuthenticatorController::OnGPMPasskeySelected(
    std::vector<uint8_t> credential_id) {
  if (transaction_started_) {
    return;
  }
  selected_cred_id_ = std::move(credential_id);

  if (account_state_ != AccountState::kLoading) {
    RecordGPMGetAssertionEvent(
        webauthn::metrics::GPMGetAssertionEvents::kStarted);
  }

  switch (account_state_) {
    case AccountState::kReady:
      model_->DisableUiOrShowLoadingDialog();
      StartTransaction();
      break;

    case AccountState::kRecoverable:
    case AccountState::kIrrecoverable:
      // TODO(crbug.com/570170982): Record a browser-authenticator event metric.
      model_->SetStep(Step::kGPMTrustThisComputerAssertion);
      break;

    case AccountState::kLoading:
      waiting_for_account_state_ = base::BindOnce(
          &GpmBrowserAuthenticatorController::OnGPMPasskeySelected,
          weak_ptr_factory_.GetWeakPtr(), *selected_cred_id_);
      OnGpmSelectedWhileLoading();
      break;

    case AccountState::kEmpty:
    case AccountState::kNone:
      model_->SetStep(Step::kGPMError);
      break;
  }
}

void GpmBrowserAuthenticatorController::OnGPMTrustThisComputer() {
  CHECK(model_->step() == Step::kGPMTrustThisComputerAssertion ||
        model_->step() == Step::kGPMTrustThisComputerCreation);
  // TODO(crbug.com/570170982): Record a browser-authenticator event metric.
  ResetDeclinedBootstrappingCount(GetProfile());
  model_->SetStep(Step::kGPMRecoverSecurityDomain);
}

void GpmBrowserAuthenticatorController::OnGPMCreationConfirmed() {
  if (transaction_started_) {
    return;
  }
  CHECK_EQ(model_->step(), Step::kGPMCreatePasskey);
  CHECK_EQ(account_state_, AccountState::kReady);
  model_->DisableUiOrShowLoadingDialog();
  StartTransaction();
}

void GpmBrowserAuthenticatorController::OnGPMPasskeysReset(bool success) {
  CHECK_EQ(model_->step(), Step::kGPMRecoverSecurityDomain);
  if (!success ||
      model_->request_type != device::FidoRequestType::kMakeCredential) {
    model_->CancelAuthenticatorRequest();
    return;
  }
  // TODO(crbug.com/570170982): When `success` is true on a MakeCredential
  // request, set `AccountState::kEmpty` and transition to `Step::kGPMCreatePin`
  // once GpmPinService is wired up for PIN creation.
  NOTIMPLEMENTED();
}

void GpmBrowserAuthenticatorController::OnTrustedVaultKeysChanged(
    std::optional<trusted_vault::TrustedVaultUserActionTriggerForUMA> trigger) {
  if (model_->step() != Step::kGPMRecoverSecurityDomain ||
      !trusted_vault_client_) {
    return;
  }

  trusted_vault_client_observer_.Reset();
  model_->DisableUiOrShowLoadingDialog();
  webauthn::user_actions::RecordRecoverySucceeded();
  // TODO(crbug.com/570170982): Record a browser-authenticator event metric.
  webauthn::metrics::RecordGPMRecoveryEvent(
      webauthn::metrics::WebAuthenticationGPMRecoveryEvent::
          kStoreKeysFromExplicitFlowSucceeded);

  auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  trusted_vault_client_->FetchKeys(
      account,
      base::BindOnce(
          &GpmBrowserAuthenticatorController::OnKeysFetchedAfterRecovery,
          weak_ptr_factory_.GetWeakPtr()));
}

void GpmBrowserAuthenticatorController::OnTrustedVaultRecoverabilityChanged() {}

Profile* GpmBrowserAuthenticatorController::GetProfile() const {
  return Profile::FromBrowserContext(
             content::RenderFrameHost::FromID(render_frame_host_id_)
                 ->GetBrowserContext())
      ->GetOriginalProfile();
}

content::WebContents* GpmBrowserAuthenticatorController::web_contents() const {
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(render_frame_host_id_));
}

void GpmBrowserAuthenticatorController::OnKeysFetched(
    CoreAccountInfo account,
    const std::vector<std::vector<uint8_t>>& keys) {
  // TODO(crbug.com/570170982): Poll the security domain service for freshness
  // of the cached keys and plumb the current key's version.
  trusted_vault_keys_ = keys;
  if (request_type_ == device::FidoRequestType::kGetAssertion &&
      !trusted_vault_keys_.empty()) {
    SetAccountState(AccountState::kReady);
    return;
  }

  DownloadAccountState(account);
}

void GpmBrowserAuthenticatorController::DownloadAccountState(
    const CoreAccountInfo& account) {
  auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  if (!rfh) {
    SetAccountState(AccountState::kNone);
    return;
  }
  Profile* const profile = GetProfile();
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      EnclaveManagerFactory::url_loader_override();
  if (!url_loader_factory) {
    url_loader_factory = profile->GetDefaultStoragePartition()
                             ->GetURLLoaderFactoryForBrowserProcess();
  }
  std::unique_ptr<trusted_vault::TrustedVaultConnection> trusted_vault_conn =
      GpmTrustedVaultConnectionProvider::GetConnection(rfh, identity_manager,
                                                       url_loader_factory);

  auto* conn = trusted_vault_conn.get();
  download_account_state_request_ =
      conn->DownloadAuthenticationFactorsRegistrationState(
          account, trusted_vault::SecurityDomainId::kPasskeys,
          base::BindOnce(
              &GpmBrowserAuthenticatorController::OnAccountStateDownloaded,
              weak_ptr_factory_.GetWeakPtr(), std::move(trusted_vault_conn)),
          base::DoNothing());
}

void GpmBrowserAuthenticatorController::OnAccountStateDownloaded(
    std::unique_ptr<trusted_vault::TrustedVaultConnection> connection,
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        result) {
  using Result =
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;
  download_account_state_request_.reset();
  key_version_ = result.key_version;

  if (!trusted_vault_keys_.empty() && result.state != Result::State::kEmpty &&
      result.state != Result::State::kError) {
    SetAccountState(AccountState::kReady);
    return;
  }

  switch (result.state) {
    case Result::State::kError:
      SetAccountState(AccountState::kNone);
      break;
    case Result::State::kEmpty:
      SetAccountState(AccountState::kEmpty);
      break;
    case Result::State::kRecoverable:
      SetAccountState(AccountState::kRecoverable);
      break;
    case Result::State::kIrrecoverable:
      SetAccountState(AccountState::kIrrecoverable);
      break;
  }
}

void GpmBrowserAuthenticatorController::OnKeysFetchedAfterRecovery(
    const std::vector<std::vector<uint8_t>>& keys) {
  if (keys.empty()) {
    model_->SetStep(Step::kGPMError);
    return;
  }

  trusted_vault_keys_ = keys;
  SetAccountState(AccountState::kReady);
  StartTransaction();
}

void GpmBrowserAuthenticatorController::SetActive(EnclaveEnabledStatus status) {
  is_active_ = status == EnclaveEnabledStatus::kEnabled;
  if (ready_for_ui_) {
    return;
  }
  ready_for_ui_ = true;
  model_->OnGPMEnclaveEnabledStatusChanged(status);
  model_->OnGPMReadyForUI();
}

void GpmBrowserAuthenticatorController::SetAccountState(
    AccountState account_state) {
  account_state_ = account_state;
  loading_timeout_.Stop();
  if (waiting_for_account_state_) {
    std::move(waiting_for_account_state_).Run();
  }
}

void GpmBrowserAuthenticatorController::OnGpmSelectedWhileLoading() {
  CHECK(waiting_for_account_state_);
  if (model_->step() != Step::kNotStarted) {
    model_->DisableUiOrShowLoadingDialog();
    return;
  }
  loading_timeout_.Start(
      FROM_HERE, kLoadingTimeout,
      base::BindOnce(&GpmBrowserAuthenticatorController::OnLoadingTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GpmBrowserAuthenticatorController::OnLoadingTimeout() {
  // TODO(crbug.com/570170982): Record a browser-authenticator event metric.
  waiting_for_account_state_.Reset();
  model_->SetStep(Step::kMechanismSelection);
}

void GpmBrowserAuthenticatorController::StartTransaction() {
  if (transaction_started_) {
    return;
  }
  transaction_started_ = true;
  CHECK(request_callback_);
  CHECK(!trusted_vault_keys_.empty());
  ResetDeclinedBootstrappingCount(GetProfile());
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetForProfile(GetProfile());

  std::optional<GpmBrowserAuthenticatorRequest> request;
  switch (request_type_) {
    case device::FidoRequestType::kMakeCredential: {
      GpmBrowserAuthenticatorRequest::MakeCredential make_data(
          trusted_vault::TrustedVaultKeyAndVersion(trusted_vault_keys_.back(),
                                                   key_version_.value_or(1)));
      std::ranges::transform(
          passkey_model->GetPasskeys(
              rp_id_, webauthn::PasskeyModel::ShadowedCredentials::kExclude),
          std::back_inserter(make_data.existing_cred_ids),
          [](const sync_pb::WebauthnCredentialSpecifics& cred) {
            const std::string& cred_id = cred.credential_id();
            return std::vector<uint8_t>(cred_id.begin(), cred_id.end());
          });
      request.emplace(std::move(make_data));
      request->save_passkey_callback = base::BindRepeating(
          &GpmBrowserAuthenticatorController::OnPasskeyCreated,
          weak_ptr_factory_.GetWeakPtr());
      break;
    }

    case device::FidoRequestType::kGetAssertion: {
      CHECK(selected_cred_id_);
      std::vector<sync_pb::WebauthnCredentialSpecifics> credentials =
          passkey_model->GetPasskeys(
              rp_id_, webauthn::PasskeyModel::ShadowedCredentials::kExclude);
      std::optional<sync_pb::WebauthnCredentialSpecifics> selected_cred;
      for (auto& cred : credentials) {
        if (std::ranges::equal(base::as_byte_span(cred.credential_id()),
                               base::span(*selected_cred_id_))) {
          selected_cred = std::move(cred);
          break;
        }
      }
      if (!selected_cred) {
        FIDO_LOG(ERROR) << "Could not find credential";
        model_->SetStep(Step::kGPMError);
        return;
      }
      passkey_model->UpdatePasskeyTimestamp(selected_cred->credential_id(),
                                            base::Time::Now());
      request.emplace(GpmBrowserAuthenticatorRequest::GetAssertion(
          trusted_vault_keys_, std::move(*selected_cred)));
      break;
    }
  }
  // TODO(crbug.com/570170982): Perform user verification and set this based on
  // the outcome.
  request->user_verified = false;

  std::move(request_callback_).Run(std::move(*request));
}

void GpmBrowserAuthenticatorController::OnPasskeyCreated(
    sync_pb::WebauthnCredentialSpecifics passkey) {
  CHECK(webauthn::passkey_model_utils::IsGpmPasskeyValid(passkey));
  PasskeyModelFactory::GetForProfile(GetProfile())->CreatePasskey(passkey);

  if (PasswordsClientUIDelegate* manage_passwords_ui_controller =
          PasswordsClientUIDelegateFromWebContents(web_contents())) {
    manage_passwords_ui_controller->OnPasskeySaved(
        /*gpm_pin_created=*/false, rp_id_);
  }
}
