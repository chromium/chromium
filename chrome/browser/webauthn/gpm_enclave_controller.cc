// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm_enclave_controller.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/change_pin_controller_impl.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/gpm_enclave_transaction.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/trusted_vault/frontend_trusted_vault_connection.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/webauthn/core/browser/gpm_user_verification_policy.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/metrics.h"
#include "device/fido/enclave/types.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_types.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/common/chrome_version.h"
#include "components/trusted_vault/icloud_recovery_key_mac.h"
#endif  // BUILDFLAG(IS_MAC)

using Step = AuthenticatorRequestDialogModel::Step;
using ChangePinEvent = ChangePinControllerImpl::ChangePinEvent;

// These diagrams aren't exhaustive, but hopefully can help identify the control
// flow in this code, which is very callback-heavy. The "digraph" sections are
// the dot commands and the diagrams are generated from them with
// https://dot-to-ascii.ggerganov.com/
//
//
// create(), already enrolled
//
// digraph {
//   OnGPMSelected -> kGPMCreatePasskey -> OnGPMCreatePasskey
//   OnGPMCreatePasskey -> StartTransaction
//   OnGPMCreatePasskey -> kGPMEnterPin -> OnGPMPinEntered ->
//     StartTransaction
//   OnGPMCreatePasskey -> kGPMTouchID -> OnTouchIDComplete ->
//     StartTransaction
// }
//
//                           +--------------------+
//                           |   OnGPMSelected    |
//                           +--------------------+
//                             |
//                             |
//                             v
//                           +--------------------+
//                           | kGPMCreatePasskey  |
//                           +--------------------+
//                             |
//                             |
//                             v
// +-------------------+     +--------------------+
// |    kGPMTouchID    | <-- | OnGPMCreatePasskey | -+
// +-------------------+     +--------------------+  |
//   |                         |                     |
//   |                         |                     |
//   v                         v                     |
// +-------------------+     +--------------------+  |
// | OnTouchIDComplete |     |    kGPMEnterPin    |  |
// +-------------------+     +--------------------+  |
//   |                         |                     |
//   |                         |                     |
//   |                         v                     |
//   |                       +--------------------+  |
//   |                       |  OnGPMPinEntered   |  |
//   |                       +--------------------+  |
//   |                         |                     |
//   |                         |                     |
//   |                         v                     |
//   |                       +--------------------+  |
//   +---------------------> |  StartTransaction  | <+
//                           +--------------------+

// create(), empty security domain
//
// digraph {
//   OnGPMSelected -> kGPMCreatePasskey -> kGPMCreatePin -> OnGPMPinEntered ->
//     OnDeviceAdded
//   OnDeviceAdded -> StartTransaction
//   OnDeviceAdded -> kGPMTouchID -> OnTouchIDComplete -> StartTransaction
// }
//
// +-------------------------+
// |      OnGPMSelected      |
// +-------------------------+
//   |
//   |
//   v
// +-------------------------+
// |    kGPMCreatePasskey    |
// +-------------------------+
//   |
//   |
//   v
// +-------------------------+
// |      kGPMCreatePin      |
// +-------------------------+
//   |
//   |
//   v
// +-------------------------+
// |     OnGPMPinEntered     |
// +-------------------------+
//   |
//   |
//   v
// +-------------------------+
// |      OnDeviceAdded      | -+
// +-------------------------+  |
//   |                          |
//   |                          |
//   v                          |
// +-------------------------+  |
// |       kGPMTouchID       |  |
// +-------------------------+  |
//   |                          |
//   |                          |
//   v                          |
// +-------------------------+  |
// |    OnTouchIDComplete    |  |
// +-------------------------+  |
//   |                          |
//   |                          |
//   v                          |
// +-------------------------+  |
// |    StartTransaction     | <+
// +-------------------------+

// get(), already enrolled
//
// digraph {
//   OnGPMPasskeySelected -> StartTransaction
//   OnGPMPasskeySelected -> kGPMEnterPin -> OnGPMPinEntered ->
//     StartTransaction
//   OnGPMPasskeySelected -> kGPMTouchID -> OnTouchIDComplete ->
//     StartTransaction
// }
//
// +-------------------+     +----------------------+
// |    kGPMTouchID    | <-- | OnGPMPasskeySelected | -+
// +-------------------+     +----------------------+  |
//   |                         |                       |
//   |                         |                       |
//   v                         v                       |
// +-------------------+     +----------------------+  |
// | OnTouchIDComplete |     |     kGPMEnterPin     |  |
// +-------------------+     +----------------------+  |
//   |                         |                       |
//   |                         |                       |
//   |                         v                       |
//   |                       +----------------------+  |
//   |                       |   OnGPMPinEntered    |  |
//   |                       +----------------------+  |
//   |                         |                       |
//   |                         |                       |
//   |                         v                       |
//   |                       +----------------------+  |
//   +---------------------> |   StartTransaction   | <+
//                           +----------------------+

namespace {

#if BUILDFLAG(IS_MAC)
constexpr char kICloudKeychainRecoveryKeyAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING ".com.google.common.folsom";
#endif  // BUILDFLAG(IS_MAC)

// Pick an enclave user verification method for a specific request.
EnclaveUserVerificationMethod PickEnclaveUserVerificationMethod(
    device::UserVerificationRequirement uv,
    bool have_entered_pin_for_recovery,
    bool has_pin,
    EnclaveManager::UvKeyState uv_key_state,
    bool platform_has_biometrics,
    bool browser_is_app) {
  constexpr bool kIsMac = BUILDFLAG(IS_MAC);

  if (have_entered_pin_for_recovery) {
    return EnclaveUserVerificationMethod::kImplicit;
  }

  // If the platform has biometrics now, but didn't when we enrolled, we need to
  // act as if they are missing because we've no UV key to use them with.
  if (uv_key_state == EnclaveManager::UvKeyState::kNone) {
    platform_has_biometrics = false;
  }

  if (!webauthn::GpmWillDoUserVerification(uv, platform_has_biometrics)) {
    return EnclaveUserVerificationMethod::kUserPresenceOnly;
  }

  switch (uv_key_state) {
    case EnclaveManager::UvKeyState::kNone:
      if (has_pin) {
        return EnclaveUserVerificationMethod::kPIN;
      } else {
        return EnclaveUserVerificationMethod::kUnsatisfiable;
      }

    case EnclaveManager::UvKeyState::kUsesSystemUI:
      return EnclaveUserVerificationMethod::kUVKeyWithSystemUI;

    case EnclaveManager::UvKeyState::kUsesSystemUIDeferredCreation:
      return EnclaveUserVerificationMethod::kDeferredUVKeyWithSystemUI;

    case EnclaveManager::UvKeyState::kUsesChromeUI:
      if (browser_is_app && kIsMac) {
        // When running in an app (i.e. a PWA) the UI is out of process. Thus we
        // cannot position the LAAuthenticationView in the UI and so we use the
        // system Touch ID dialog instead.
        return EnclaveUserVerificationMethod::kUVKeyWithSystemUI;
      } else {
        return EnclaveUserVerificationMethod::kUVKeyWithChromeUI;
      }
  }
}

const char* ToString(
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::State
        state) {
  using Result =
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;
  switch (state) {
    case Result::State::kError:
      return "Error";
    case Result::State::kEmpty:
      return "Empty";
    case Result::State::kRecoverable:
      return "Recoverable";
    case Result::State::kIrrecoverable:
      return "Irrecoverable";
  }
}

void ResetDeclinedBootstrappingCount(Profile* profile) {
  profile->GetPrefs()->SetInteger(
      webauthn::pref_names::kEnclaveDeclinedGPMBootstrappingCount, 0);
  profile->GetPrefs()->SetInteger(
      webauthn::pref_names::kEnclaveDeclinedGPMCredentialCreationCount, 0);
}

}  // namespace

GpmTrustedVaultConnectionProvider::GpmTrustedVaultConnectionProvider(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<GpmTrustedVaultConnectionProvider>(rfh) {}

GpmTrustedVaultConnectionProvider::~GpmTrustedVaultConnectionProvider() =
    default;

// static
void GpmTrustedVaultConnectionProvider::SetOverrideForFrame(
    content::RenderFrameHost* rfh,
    std::unique_ptr<trusted_vault::TrustedVaultConnection>
        connection_override) {
  if (!rfh) {
    return;
  }
  GpmTrustedVaultConnectionProvider* provider =
      GetOrCreateForCurrentDocument(rfh);
  provider->connection_override_ = std::move(connection_override);
}

// static
std::unique_ptr<trusted_vault::TrustedVaultConnection>
GpmTrustedVaultConnectionProvider::GetConnection(
    content::RenderFrameHost* rfh,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (rfh) {
    if (GpmTrustedVaultConnectionProvider* provider =
            GetForCurrentDocument(rfh)) {
      if (provider->connection_override_) {
        return std::move(provider->connection_override_);
      }
    }
  }

  // Default creation logic
  return trusted_vault::NewFrontendTrustedVaultConnection(
      trusted_vault::SecurityDomainId::kPasskeys, identity_manager,
      url_loader_factory);
}

DOCUMENT_USER_DATA_KEY_IMPL(GpmTrustedVaultConnectionProvider);

GpmTickAndTaskRunnerProvider::GpmTickAndTaskRunnerProvider(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<GpmTickAndTaskRunnerProvider>(rfh) {}

GpmTickAndTaskRunnerProvider::~GpmTickAndTaskRunnerProvider() = default;

// static
void GpmTickAndTaskRunnerProvider::SetOverrideForFrame(
    content::RenderFrameHost* rfh,
    base::TickClock const* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  if (!rfh) {
    return;
  }
  GpmTickAndTaskRunnerProvider* provider = GetOrCreateForCurrentDocument(rfh);
  provider->tick_clock_ = tick_clock;
  provider->task_runner_ = std::move(task_runner);
}

// static
base::TickClock const* GpmTickAndTaskRunnerProvider::GetTickClock(
    content::RenderFrameHost* rfh) {
  if (rfh) {
    if (GpmTickAndTaskRunnerProvider* provider = GetForCurrentDocument(rfh)) {
      return provider->tick_clock_;
    }
  }
  return base::DefaultTickClock::GetInstance();
}

// static
scoped_refptr<base::SequencedTaskRunner>
GpmTickAndTaskRunnerProvider::GetTaskRunner(content::RenderFrameHost* rfh) {
  if (rfh) {
    if (GpmTickAndTaskRunnerProvider* provider = GetForCurrentDocument(rfh)) {
      return provider->task_runner_;
    }
  }
  return nullptr;
}

DOCUMENT_USER_DATA_KEY_IMPL(GpmTickAndTaskRunnerProvider);

GPMEnclaveController::GPMEnclaveController(
    content::RenderFrameHost* render_frame_host,
    AuthenticatorRequestDialogModel* model,
    const std::string& rp_id,
    device::FidoRequestType request_type,
    device::UserVerificationRequirement user_verification_requirement)
    : render_frame_host_id_(render_frame_host->GetGlobalId()),
      rp_id_(rp_id),
      request_type_(request_type),
      user_verification_requirement_(user_verification_requirement),
      enclave_manager_(
          EnclaveManagerFactory::GetAsEnclaveManagerForProfile(GetProfile())),
      model_(model) {
  enclave_manager_observer_.Observe(enclave_manager_);
  model_observer_.Observe(model_);

  Profile* const profile = GetProfile();
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetInstance()->GetForProfile(profile);
  creds_ = passkey_model->GetPasskeysForRelyingPartyId(rp_id_);
  if (base::FeatureList::IsEnabled(device::kWebAuthnSignalApiHidePasskeys)) {
    std::erase_if(creds_, [](const auto& cred) { return cred.hidden(); });
  }

  // The following code may do some asynchronous processing. However the control
  // flow terminates, it must have called SetAccountState with some value.
  if (creds_.empty() &&
      request_type == device::FidoRequestType::kGetAssertion) {
    // No possibility of using GPM for this request.
    FIDO_LOG(EVENT) << "Enclave is not a candidate for this request";
    SetAccountState(AccountState::kNone);
    SetActive(EnclaveEnabledStatus::kDisabled);
    return;
  }

  // Verify the state of the primary account sign-in info.
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  GoogleServiceAuthError signin_error =
      identity_manager->GetErrorStateOfRefreshTokenForAccount(
          account.account_id);
  if (signin_error.IsPersistentError()) {
    FIDO_LOG(EVENT) << "Recoverable sign-in error: " << signin_error.ToString();
    device::enclave::RecordEvent(device::enclave::Event::kEnclaveReauthNeeded);
    SetAccountState(AccountState::kNone);
    SetActive(EnclaveEnabledStatus::kEnabledAndReauthNeeded);
    return;
  }
  SetActive(EnclaveEnabledStatus::kEnabled);
  if (enclave_manager_->is_loaded()) {
    OnEnclaveLoaded();
  } else {
    FIDO_LOG(EVENT) << "Loading enclave state";
    enclave_manager_->Load(
        base::BindOnce(&GPMEnclaveController::OnEnclaveLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

GPMEnclaveController::~GPMEnclaveController() {
  // Ensure that any secret is dropped from memory after a transaction.
  enclave_manager_->TakeSecret();
}

std::optional<EnclaveUserVerificationMethod>
GPMEnclaveController::GetEnclaveUserVerificationMethod() {
  // TODO(crbug.com/393055190): Figure out why `ready_for_ui` is not enough for
  // `is_ready`.
  if (!enclave_manager_->is_ready()) {
    return std::nullopt;
  }

  bool has_pin = enclave_manager_->has_wrapped_pin();
  EnclaveManager::UvKeyState uv_key_state = enclave_manager_->uv_key_state(
      model_->platform_has_biometrics.value_or(false));

  return PickEnclaveUserVerificationMethod(
      user_verification_requirement_, /*have_entered_pin_for_recovery=*/false,
      has_pin, uv_key_state, model_->platform_has_biometrics.value_or(false),
      BrowserIsApp());
}

bool GPMEnclaveController::is_active() const {
  return *is_active_;
}

bool GPMEnclaveController::ready_for_ui() const {
  return ready_for_ui_;
}

void GPMEnclaveController::ConfigureDiscoveries(
    device::FidoDiscoveryFactory* discovery_factory) {
  using EnclaveEventStream = device::FidoDiscoveryBase::EventStream<
      std::unique_ptr<device::enclave::CredentialRequest>>;
  std::unique_ptr<EnclaveEventStream> event_stream;
  std::tie(enclave_request_callback_, event_stream) = EnclaveEventStream::New();
  discovery_factory->set_enclave_ui_request_stream(std::move(event_stream));
}

const std::vector<sync_pb::WebauthnCredentialSpecifics>&
GPMEnclaveController::creds() const {
  return creds_;
}

void GPMEnclaveController::HandleEnclaveTransactionError() {
  model_->SetStep(Step::kGPMError);
}

void GPMEnclaveController::BuildUVKeyOptions(
    EnclaveManager::UVKeyOptions& uv_options) {
  uv_options.rp_id = rp_id_;
  uv_options.render_frame_host_id = render_frame_host_id_;
  uv_options.local_auth_token = std::move(model_->local_auth_token);
#if BUILDFLAG(IS_CHROMEOS)
  if (ash::features::IsWebAuthNAuthDialogMergeEnabled()) {
    uv_options.dialog_controller = ash::ActiveSessionAuthController::Get();
  } else {
    uv_options.dialog_controller = ash::WebAuthNDialogController::Get();
  }
#endif
}

void GPMEnclaveController::OnPasskeyCreated(
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  if (manage_passwords_ui_controller) {
    bool gpm_pin_created_in_this_request =
        gpm_pin_creation_confirmed_ && enclave_manager_->has_wrapped_pin();
    manage_passwords_ui_controller->OnPasskeySaved(
        gpm_pin_created_in_this_request, rp_id_);
  }
}

EnclaveUserVerificationMethod GPMEnclaveController::GetUvMethod() {
  uv_method_ = PickEnclaveUserVerificationMethod(
      user_verification_requirement_,
      have_added_device_ && !recovered_with_icloud_keychain_,
      enclave_manager_->has_wrapped_pin(),
      enclave_manager_->uv_key_state(*model_->platform_has_biometrics),
      *model_->platform_has_biometrics, BrowserIsApp());
  return *uv_method_;
}

Profile* GPMEnclaveController::GetProfile() const {
  return Profile::FromBrowserContext(
             content::RenderFrameHost::FromID(render_frame_host_id_)
                 ->GetBrowserContext())
      ->GetOriginalProfile();
}

void GPMEnclaveController::ShowSecurityDomainRecoveryUI() {
  if (ShouldRefreshState()) {
    RefreshStateAndRepeatOperation();
    return;
  }
  // The acquired lock indicates that the explicit key retrieval flow is being
  // used.
  store_keys_lock_ = enclave_manager_->GetStoreKeysLock();
  model_->SetStep(Step::kRecoverSecurityDomain);
}

GPMEnclaveController::AccountState
GPMEnclaveController::account_state_for_testing() const {
  return account_state_;
}

GPMEnclaveController::AccountReadyState
GPMEnclaveController::account_ready_state() const {
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

void GPMEnclaveController::RunWhenAccountReady(base::OnceClosure callback) {
  if (account_state_ != AccountState::kLoading) {
    std::move(callback).Run();
    return;
  }

  CHECK(!waiting_for_account_state_);
  waiting_for_account_state_ = std::move(callback);
}

void GPMEnclaveController::OnEnclaveLoaded() {
  // For create() requests, we want to probe the security domain service to
  // ensure that we never create a credential encrypted to an old security
  // domain secret. For get() requests, we can generally skip the probe if
  // already enrolled. However, if this request will require using the GPM PIN
  // for UV then we want to probe to ensure that we catch any changes to the GPM
  // PIN. While we cannot fully determine the UV method at this stage, because
  // we don't know whether the platform has biometrics, we can know whether
  // we'll use a GPM PIN for UV or not.
  if (request_type_ == device::FidoRequestType::kGetAssertion) {
    if (enclave_manager_->is_ready()) {
      switch (PickEnclaveUserVerificationMethod(
          user_verification_requirement_,
          /*have_entered_pin_for_recovery=*/false,
          enclave_manager_->has_wrapped_pin(),
          enclave_manager_->uv_key_state(/*platform_has_biometrics=*/false),
          /*platform_has_biometrics=*/false, BrowserIsApp())) {
        case EnclaveUserVerificationMethod::kPIN:
          FIDO_LOG(EVENT)
              << "Checking security domain service because a GPM PIN will be "
                 "used for user verification in this request.";
          break;
        default:
          FIDO_LOG(EVENT) << "Enclave is ready and this request will not use a "
                             "GPM PIN for user verification";
          SetAccountState(AccountState::kReady);
          SetActive(EnclaveEnabledStatus::kEnabled);
          return;
      }
    }

  }

  FIDO_LOG(EVENT) << "Checking for UV key capability";
  EnclaveManager::AreUserVerifyingKeysSupported(
      base::BindOnce(&GPMEnclaveController::OnUVCapabilityKnown,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GPMEnclaveController::OnUVCapabilityKnown(bool can_make_uv_keys) {
  FIDO_LOG(EVENT) << "UV key capability: " << can_make_uv_keys;
  can_make_uv_keys_ = can_make_uv_keys;
  DownloadAccountState();
}

void GPMEnclaveController::DownloadAccountState() {
  FIDO_LOG(EVENT) << "Fetching account state";

  auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  scoped_refptr<network::SharedURLLoaderFactory> testing_url_loader =
      EnclaveManagerFactory::url_loader_override();
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      testing_url_loader ? testing_url_loader
                         : SystemNetworkContextManager::GetInstance()
                               ->GetSharedURLLoaderFactory();
  std::unique_ptr<trusted_vault::TrustedVaultConnection> trusted_vault_conn =
      GpmTrustedVaultConnectionProvider::GetConnection(rfh, identity_manager,
                                                       url_loader_factory);

  auto* conn = trusted_vault_conn.get();
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  download_account_state_request_ =
      conn->DownloadAuthenticationFactorsRegistrationState(
          account,
          base::BindOnce(&GPMEnclaveController::OnAccountStateDownloaded,
                         weak_ptr_factory_.GetWeakPtr(), account.gaia,
                         std::move(trusted_vault_conn)),
          base::DoNothing());
}

void GPMEnclaveController::OnAccountStateDownloaded(
    GaiaId gaia_id,
    std::unique_ptr<trusted_vault::TrustedVaultConnection> unused,
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        result) {
  using Result =
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;
  download_account_state_request_.reset();

  FIDO_LOG(EVENT)
      << "Download account state result: " << ToString(result.state)
      << ", key_version: " << result.key_version.value_or(0)
      << ", has PIN: " << result.gpm_pin_metadata.has_value() << ", expiry: "
      << (result.gpm_pin_metadata &&
                  result.gpm_pin_metadata->usable_pin_metadata
              ? base::TimeFormatAsIso8601(
                    result.gpm_pin_metadata->usable_pin_metadata->expiry)
              : "<none>")
      << ", iCloud Keychain keys: " << result.icloud_keys.size();

  if (enclave_manager_->is_ready() &&
      enclave_manager_->ConsiderSecurityDomainState(result,
                                                    base::DoNothing())) {
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
  pin_metadata_ = std::move(result.gpm_pin_metadata);
  security_domain_icloud_recovery_keys_ = std::move(result.icloud_keys);
  user_gaia_id_ = std::move(gaia_id);
}

void GPMEnclaveController::SetActive(EnclaveEnabledStatus status) {
  is_active_ = status == EnclaveEnabledStatus::kEnabled;
  if (ready_for_ui_) {
    return;
  }
  ready_for_ui_ = true;
  model_->EnclaveEnabledStatusChanged(status);
  model_->OnReadyForUI();
}

void GPMEnclaveController::RefreshStateAndRepeatOperation() {
  is_state_stale_ = false;
  // The account state is stale. Reload it and then restart the operation.
  waiting_for_account_state_ =
      request_type_ == device::FidoRequestType::kGetAssertion
          ? base::BindOnce(&GPMEnclaveController::OnGPMPasskeySelected,
                           weak_ptr_factory_.GetWeakPtr(), *selected_cred_id_)
          : base::BindOnce(&GPMEnclaveController::OnGPMSelected,
                           weak_ptr_factory_.GetWeakPtr());
  // Refreshing the state:
  SetAccountState(AccountState::kLoading);
  OnEnclaveLoaded();
}

bool GPMEnclaveController::ShouldRefreshState() {
  if (!base::FeatureList::IsEnabled(
          device::kWebAuthnEnableRefreshingStateOfGpmEnclaveController)) {
    return false;
  }
  // In case of removing passkey access Enclave Manager might become
  // unregistered but GPM Enclave Controller might still be in the active
  // state.
  bool account_state_is_out_of_sync =
      account_state_ == AccountState::kReady && !enclave_manager_->is_ready();
  return is_state_stale_ || account_state_is_out_of_sync;
}

void GPMEnclaveController::OnOutOfContextRecoveryCompletion(
    EnclaveManager::OutOfContextRecoveryOutcome outcome) {
  if (outcome == EnclaveManager::OutOfContextRecoveryOutcome::
                     kStoreKeysFromOpportunisticFlowSucceeded) {
    // In case of successful opportunistic key retrieval we conclude
    // that the state of GPM Enclave Controller becomes stale.
    is_state_stale_ = true;
  }
}

void GPMEnclaveController::OnKeysStored() {
  if (recovered_with_icloud_keychain_) {
    // iCloud keychain recovery.
    device::enclave::RecordEvent(
        device::enclave::Event::kICloudRecoverySuccessful);
  } else if (model_->step() == Step::kRecoverSecurityDomain) {
    // MagicArch recovery.
    webauthn::user_actions::RecordRecoverySucceeded();
    device::enclave::RecordEvent(device::enclave::Event::kRecoverySuccessful);
    webauthn::metrics::RecordGPMRecoveryEvent(
        webauthn::metrics::WebAuthenticationGPMRecoveryEvent::
            kStoreKeysFromExplicitFlowSucceeded);
  } else {
    // Keys were stored but we were not expecting it, e.g. because it happened
    // during a request at a different step on another tab. In case of
    // successful key retrieval in another tab, we conclude that the state of
    // the GPM Enclave Controller has become stale.
    is_state_stale_ = true;
    return;
  }

  CHECK(enclave_manager_->has_pending_keys());
  CHECK(!enclave_manager_->is_ready());
  store_keys_lock_.reset();

  if ((pin_metadata_.has_value() && pin_metadata_->usable_pin_metadata) ||
      *can_make_uv_keys_) {
    // No need to create a GPM PIN if the user already has a usable GPM PIN or
    // can make UV keys.
    if (!enclave_manager_->AddDeviceToAccount(
            std::move(pin_metadata_),
            base::BindOnce(&GPMEnclaveController::OnDeviceAdded,
                           weak_ptr_factory_.GetWeakPtr()))) {
      model_->SetStep(Step::kGPMError);
    }
  } else {
    // Create a GPM PIN if the user doesn't have one (or it cannot be used) and
    // can't make a UV key locally.
    model_->SetStep(Step::kGPMCreatePin);
  }
}

void GPMEnclaveController::OnDeviceAdded(bool success) {
  ResetDeclinedBootstrappingCount(GetProfile());
  if (!success) {
    model_->SetStep(Step::kGPMError);
    return;
  }

#if BUILDFLAG(IS_MAC)
  MaybeAddICloudRecoveryKey();
#else
  OnEnclaveAccountSetUpComplete();
#endif  // BUILDFLAG(IS_MAC)
}

void GPMEnclaveController::RecoverSecurityDomain() {
#if BUILDFLAG(IS_MAC)
  model_->DisableUiOrShowLoadingDialog();
  trusted_vault::ICloudRecoveryKey::Retrieve(
      base::BindOnce(&GPMEnclaveController::OnICloudKeysRetrievedForRecovery,
                     weak_ptr_factory_.GetWeakPtr()),
      trusted_vault::SecurityDomainId::kPasskeys,
      kICloudKeychainRecoveryKeyAccessGroup);
#else
  ShowSecurityDomainRecoveryUI();
#endif  // BUILDFLAG(IS_MAC)
}

#if BUILDFLAG(IS_MAC)

void GPMEnclaveController::MaybeAddICloudRecoveryKey() {
  trusted_vault::ICloudRecoveryKey::Retrieve(
      base::BindOnce(&GPMEnclaveController::OnICloudKeysRetrievedForEnrollment,
                     weak_ptr_factory_.GetWeakPtr()),
      trusted_vault::SecurityDomainId::kPasskeys,
      kICloudKeychainRecoveryKeyAccessGroup);
}

void GPMEnclaveController::OnICloudKeysRetrievedForEnrollment(
    std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>
        local_icloud_keys) {
  for (const trusted_vault::VaultMember& recovery_icloud_key :
       security_domain_icloud_recovery_keys_) {
    std::vector<uint8_t> public_key =
        recovery_icloud_key.public_key->ExportToBytes();
    const auto local_icloud_key_it = std::ranges::find_if(
        local_icloud_keys,
        [&public_key](const auto& key) { return key->id() == public_key; });
    if (local_icloud_key_it != local_icloud_keys.end()) {
      // This device already has an iCloud keychain recovery factor configured
      // for the passkey security domain. Nothing else to do here.
      FIDO_LOG(EVENT) << "Device already has iCloud recovery key configured";
      OnEnclaveAccountSetUpComplete();
      return;
    }
  }

  // The device has no iCloud recovery key for the passkeys security domain.
  // Create a new key.
  // TODO(nsatragno): it's possible we might want to share keys with other
  // security domains. We would need to loop through all vault members across
  // all security domains.
  FIDO_LOG(EVENT) << "Creating new iCloud recovery key";
  trusted_vault::ICloudRecoveryKey::Create(
      base::BindOnce(&GPMEnclaveController::EnrollICloudRecoveryKey,
                     weak_ptr_factory_.GetWeakPtr()),
      trusted_vault::SecurityDomainId::kPasskeys,
      kICloudKeychainRecoveryKeyAccessGroup);
}

void GPMEnclaveController::EnrollICloudRecoveryKey(
    std::unique_ptr<trusted_vault::ICloudRecoveryKey> key) {
  if (!key) {
    FIDO_LOG(ERROR) << "Could not create iCloud recovery key";
    OnEnclaveAccountSetUpComplete();
    return;
  }
  enclave_manager_->AddICloudRecoveryKey(
      std::move(key), base::IgnoreArgs<bool>(base::BindOnce(
                          &GPMEnclaveController::OnEnclaveAccountSetUpComplete,
                          weak_ptr_factory_.GetWeakPtr())));
}

void GPMEnclaveController::OnICloudKeysRetrievedForRecovery(
    std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>
        local_icloud_keys) {
  // Find the matching pair of local iCloud private key and the SDS recovery
  // member.
  auto local_icloud_key_it = local_icloud_keys.end();
  auto recovery_icloud_key_it = std::ranges::find_if(
      security_domain_icloud_recovery_keys_,
      [&local_icloud_key_it,
       &local_icloud_keys](const auto& recovery_icloud_key) {
        std::vector<uint8_t> public_key =
            recovery_icloud_key.public_key->ExportToBytes();
        local_icloud_key_it = std::ranges::find_if(
            local_icloud_keys,
            [&public_key](const auto& key) { return key->id() == public_key; });
        return local_icloud_key_it != local_icloud_keys.end();
      });
  if (local_icloud_key_it == local_icloud_keys.end()) {
    FIDO_LOG(DEBUG) << "Could not find matching iCloud recovery key";
    ShowSecurityDomainRecoveryUI();
    return;
  }
  const auto member_key_it = std::ranges::max_element(
      recovery_icloud_key_it->member_keys,
      [](const auto& k1, const auto& k2) { return k1.version < k2.version; });
  std::optional<std::vector<uint8_t>> security_domain_secret =
      trusted_vault::DecryptTrustedVaultWrappedKey(
          (*local_icloud_key_it)->key()->private_key(),
          member_key_it->wrapped_key);
  if (!security_domain_secret) {
    FIDO_LOG(ERROR)
        << "Could not decrypt security domain secret with iCloud key";
    ShowSecurityDomainRecoveryUI();
    return;
  }
  FIDO_LOG(EVENT) << "Successful recovery from iCloud recovery key";
  recovered_with_icloud_keychain_ = true;
  store_keys_lock_ = enclave_manager_->GetStoreKeysLock();
  enclave_manager_->StoreKeys(user_gaia_id_,
                              {std::move(*security_domain_secret)},
                              member_key_it->version);
}

#endif  // BUILDFLAG(IS_MAC)

void GPMEnclaveController::OnEnclaveAccountSetUpComplete() {
  have_added_device_ = true;
  SetAccountState(AccountState::kReady);
  SetFailedPINAttemptCount(0);

  uv_method_ = PickEnclaveUserVerificationMethod(
      user_verification_requirement_,
      have_added_device_ && !recovered_with_icloud_keychain_,
      enclave_manager_->has_wrapped_pin(),
      enclave_manager_->uv_key_state(*model_->platform_has_biometrics),
      *model_->platform_has_biometrics, BrowserIsApp());
  switch (*uv_method_) {
    case EnclaveUserVerificationMethod::kUVKeyWithSystemUI:
    case EnclaveUserVerificationMethod::kDeferredUVKeyWithSystemUI:
    case EnclaveUserVerificationMethod::kUserPresenceOnly:
    case EnclaveUserVerificationMethod::kImplicit:
      model_->DisableUiOrShowLoadingDialog();
      StartTransaction();
      break;

    case EnclaveUserVerificationMethod::kUVKeyWithChromeUI:
      model_->SetStep(Step::kGPMTouchID);
      break;

    case EnclaveUserVerificationMethod::kUnsatisfiable:
      // TODO(crbug.com/367985619): it's possible to get to this state if a user
      // recovers from iCloud keychain and does not have a usable PIN.
      model_->SetStep(Step::kGPMError);
      break;

    case EnclaveUserVerificationMethod::kPIN:
      PromptForPin();
      break;

    case EnclaveUserVerificationMethod::kNoUserVerificationAndNoUserPresence:
      NOTREACHED();  // Only valid for passkey upgrade requests.
  }
}

void GPMEnclaveController::SetAccountState(AccountState account_state) {
  account_state_ = account_state;
  if (account_state_ == AccountState::kReady) {
    pin_is_arbitrary_ = enclave_manager_->has_wrapped_pin() &&
                        enclave_manager_->wrapped_pin_is_arbitrary();
  }
  loading_timeout_.Stop();
  if (waiting_for_account_state_) {
    std::move(waiting_for_account_state_).Run();
  }
}

void GPMEnclaveController::PromptForPin() {
  if (GetFailedPINAttemptCount() >= device::enclave::kMaxFailedPINAttempts) {
    model_->SetStep(Step::kGPMLockedPin);
  } else {
    model_->SetStep(pin_is_arbitrary_ ? Step::kGPMEnterArbitraryPin
                                      : Step::kGPMEnterPin);
  }
}

void GPMEnclaveController::OnGpmPinChanged(bool success) {
  changing_gpm_pin_ = false;

  if (!success) {
    model_->SetStep(Step::kGPMError);
    ChangePinControllerImpl::RecordHistogram(ChangePinEvent::kFailed);
    return;
  }

  SetFailedPINAttemptCount(0);
  model_->gpm_pin_remaining_attempts_ = std::nullopt;
  // Changing GPM Pin required reauth, hence we can just proceed with the
  // get/create passkey transaction.
  StartTransaction();
  ChangePinControllerImpl::RecordHistogram(
      ChangePinEvent::kCompletedSuccessfully);
}

void GPMEnclaveController::OnGpmSelectedWhileLoading() {
  CHECK(waiting_for_account_state_);
  if (model_->step() != AuthenticatorRequestDialogModel::Step::kNotStarted) {
    model_->DisableUiOrShowLoadingDialog();
    return;
  }
  loading_timeout_.Start(FROM_HERE, kLoadingTimeout,
                         base::BindOnce(&GPMEnclaveController::OnLoadingTimeout,
                                        weak_ptr_factory_.GetWeakPtr()));
  return;
}

void GPMEnclaveController::OnLoadingTimeout() {
  device::enclave::RecordEvent(device::enclave::Event::kLoadingTimeout);
  waiting_for_account_state_.Reset();
  model_->SetStep(AuthenticatorRequestDialogModel::Step::kMechanismSelection);
}


void GPMEnclaveController::OnGPMSelected() {
  if (ShouldRefreshState()) {
    RefreshStateAndRepeatOperation();
    return;
  }

  // Reset after each GPM selection to ensure correct metric emission.
  model_->in_onboarding_flow = false;

  if (model_->is_off_the_record && !off_the_record_confirmed_) {
    model_->SetStep(Step::kGPMConfirmOffTheRecordCreate);
    return;
  }

  if (account_state_ != AccountState::kLoading) {
    // `kLoading` will call `OnGPMSelected` again, therefore we don't emit in
    // these states.
    RecordGPMMakeCredentialEvent(
        webauthn::metrics::GPMMakeCredentialEvents::kStarted);
  }

  switch (account_state_) {
    case AccountState::kEmpty:
      // Set to true to indicate that the user has entered the GPM onboarding
      // flow. This enables emission of onboarding-specific metrics.
      model_->in_onboarding_flow = true;
      RecordOnboardingEvent(webauthn::metrics::OnboardingEvents::kStarted);
      model_->SetStep(Step::kGPMCreatePasskey);
      break;

    case AccountState::kReady:
      uv_method_ = PickEnclaveUserVerificationMethod(
          user_verification_requirement_,
          have_added_device_ && !recovered_with_icloud_keychain_,
          enclave_manager_->has_wrapped_pin(),
          enclave_manager_->uv_key_state(*model_->platform_has_biometrics),
          *model_->platform_has_biometrics, BrowserIsApp());

      switch (*uv_method_) {
        case EnclaveUserVerificationMethod::kUVKeyWithSystemUI:
        case EnclaveUserVerificationMethod::kDeferredUVKeyWithSystemUI:
        case EnclaveUserVerificationMethod::kUserPresenceOnly:
        case EnclaveUserVerificationMethod::kImplicit:
        case EnclaveUserVerificationMethod::kPIN:
          model_->SetStep(Step::kGPMCreatePasskey);
          break;

        case EnclaveUserVerificationMethod::kUVKeyWithChromeUI:
          model_->SetStep(Step::kGPMTouchID);
          break;

        case EnclaveUserVerificationMethod::kUnsatisfiable:
          model_->SetStep(Step::kGPMError);
          break;

        case EnclaveUserVerificationMethod::
            kNoUserVerificationAndNoUserPresence:
          NOTREACHED();  // Only valid for passkey upgrade requests.
      }
      break;

    case AccountState::kRecoverable:
    case AccountState::kIrrecoverable:
      device::enclave::RecordEvent(device::enclave::Event::kOnboarding);
      model_->SetStep(Step::kTrustThisComputerCreation);
      break;

    case AccountState::kLoading:
      waiting_for_account_state_ = base::BindOnce(
          &GPMEnclaveController::OnGPMSelected, weak_ptr_factory_.GetWeakPtr());
      OnGpmSelectedWhileLoading();
      break;

    case AccountState::kNone:
      model_->SetStep(Step::kGPMError);
      break;
  }
}

void GPMEnclaveController::OnGPMPasskeySelected(
    std::vector<uint8_t> credential_id) {
  selected_cred_id_ = std::move(credential_id);

  if (ShouldRefreshState()) {
    RefreshStateAndRepeatOperation();
    return;
  }

  if (account_state_ != AccountState::kLoading) {
    // `kLoading` will call `OnGPMPasskeySelected` again, therefore we don't
    // emit in these states.
    RecordGPMGetAssertionEvent(
        webauthn::metrics::GPMGetAssertionEvents::kStarted);
  }

  switch (account_state_) {
    case AccountState::kReady:
      uv_method_ = PickEnclaveUserVerificationMethod(
          user_verification_requirement_,
          have_added_device_ && !recovered_with_icloud_keychain_,
          enclave_manager_->has_wrapped_pin(),
          enclave_manager_->uv_key_state(*model_->platform_has_biometrics),
          *model_->platform_has_biometrics, BrowserIsApp());

      switch (*uv_method_) {
        case EnclaveUserVerificationMethod::kUVKeyWithSystemUI:
        case EnclaveUserVerificationMethod::kDeferredUVKeyWithSystemUI:
        case EnclaveUserVerificationMethod::kUserPresenceOnly:
        case EnclaveUserVerificationMethod::kImplicit:
          model_->DisableUiOrShowLoadingDialog();
          StartTransaction();
          break;

        case EnclaveUserVerificationMethod::kPIN:
          PromptForPin();
          break;

        case EnclaveUserVerificationMethod::kUVKeyWithChromeUI:
          model_->SetStep(Step::kGPMTouchID);
          break;

        case EnclaveUserVerificationMethod::kUnsatisfiable:
          model_->SetStep(Step::kGPMError);
          break;

        case EnclaveUserVerificationMethod::
            kNoUserVerificationAndNoUserPresence:
          NOTREACHED();  // Only valid for passkey upgrade requests.
      }
      break;

    case AccountState::kRecoverable:
    case AccountState::kIrrecoverable:
      device::enclave::RecordEvent(device::enclave::Event::kOnboarding);
      model_->SetStep(Step::kTrustThisComputerAssertion);
      break;

    case AccountState::kLoading:
      waiting_for_account_state_ =
          base::BindOnce(&GPMEnclaveController::OnGPMPasskeySelected,
                         weak_ptr_factory_.GetWeakPtr(), *selected_cred_id_);
      OnGpmSelectedWhileLoading();
      break;

    case AccountState::kNone:
      // This can happen if a passkey is selected after the enclave times out.
      model_->SetStep(Step::kGPMError);
      break;

    case AccountState::kEmpty:
      // The security domain is empty but there were
      // sync entities. Most like the security domain was reset without
      // clearing the entities, thus they are unusable.
      model_->SetStep(Step::kGPMError);
      break;
  }
}

void GPMEnclaveController::OnTrustThisComputer() {
  CHECK(model_->step() == Step::kTrustThisComputerAssertion ||
        model_->step() == Step::kTrustThisComputerCreation);
  device::enclave::RecordEvent(device::enclave::Event::kOnboardingAccepted);
  // Clicking through the bootstrapping dialog resets the count even if it
  // doesn't end up being successful.
  ResetDeclinedBootstrappingCount(GetProfile());
  RecoverSecurityDomain();
}

void GPMEnclaveController::OnGPMPinOptionChanged(bool is_arbitrary) {
  if (changing_gpm_pin_) {
    CHECK(model_->step() == Step::kGPMChangePin ||
          model_->step() == Step::kGPMChangeArbitraryPin);
    model_->SetStep(is_arbitrary ? Step::kGPMChangeArbitraryPin
                                 : Step::kGPMChangePin);
  } else {
    CHECK(model_->step() == Step::kGPMCreatePin ||
          model_->step() == Step::kGPMCreateArbitraryPin);
    model_->SetStep(is_arbitrary ? Step::kGPMCreateArbitraryPin
                                 : Step::kGPMCreatePin);
  }
}

void GPMEnclaveController::OnGPMCreatePasskey() {
  if (ShouldRefreshState()) {
    RefreshStateAndRepeatOperation();
    return;
  }
  CHECK_EQ(model_->step(), Step::kGPMCreatePasskey);
  CHECK(account_state_ == AccountState::kEmpty ||
        account_state_ == AccountState::kReady);
  if (account_state_ == AccountState::kEmpty) {
    model_->SetStep(Step::kGPMCreatePin);
  } else {
    switch (*uv_method_) {
      case EnclaveUserVerificationMethod::kUVKeyWithSystemUI:
      case EnclaveUserVerificationMethod::kDeferredUVKeyWithSystemUI:
      case EnclaveUserVerificationMethod::kUserPresenceOnly:
      case EnclaveUserVerificationMethod::kImplicit:
        model_->DisableUiOrShowLoadingDialog();
        StartTransaction();
        break;

      case EnclaveUserVerificationMethod::kPIN:
        PromptForPin();
        break;

      case EnclaveUserVerificationMethod::kUVKeyWithChromeUI:
        model_->SetStep(Step::kGPMTouchID);
        break;

      case EnclaveUserVerificationMethod::kNoUserVerificationAndNoUserPresence:
      case EnclaveUserVerificationMethod::kUnsatisfiable:
        NOTREACHED();
    }
  }
}

void GPMEnclaveController::OnGPMConfirmOffTheRecordCreate() {
  CHECK_EQ(model_->step(), Step::kGPMConfirmOffTheRecordCreate);
  off_the_record_confirmed_ = true;
  OnGPMSelected();
}

void GPMEnclaveController::OnGPMPinEntered(const std::u16string& pin) {
  if (ShouldRefreshState()) {
    RefreshStateAndRepeatOperation();
    return;
  }
  CHECK(model_->step() == Step::kGPMChangeArbitraryPin ||
        model_->step() == Step::kGPMChangePin ||
        model_->step() == Step::kGPMCreateArbitraryPin ||
        model_->step() == Step::kGPMCreatePin ||
        model_->step() == Step::kGPMEnterArbitraryPin ||
        model_->step() == Step::kGPMEnterPin);
  pin_ = base::UTF16ToUTF8(pin);

  // Disable the pin entry view while waiting for the response from enclave.
  model_->DisableUiOrShowLoadingDialog();

  if (model_->step() == Step::kGPMChangeArbitraryPin ||
      model_->step() == Step::kGPMChangePin ||
      model_->step() == Step::kGPMCreateArbitraryPin ||
      model_->step() == Step::kGPMCreatePin) {
    gpm_pin_creation_confirmed_ = true;
  }

  if (account_state_ == AccountState::kRecoverable) {
    CHECK(enclave_manager_->has_pending_keys());
    // In this case, we were waiting for the user to create their GPM PIN.
    enclave_manager_->AddDeviceAndPINToAccount(
        *pin_, pin_metadata_ ? pin_metadata_->public_key : std::nullopt,
        base::BindOnce(&GPMEnclaveController::OnDeviceAdded,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (account_state_ == AccountState::kEmpty) {
    // The user has set a PIN to create the account.
    enclave_manager_->SetupWithPIN(
        *pin_, base::BindOnce(&GPMEnclaveController::OnDeviceAdded,
                              weak_ptr_factory_.GetWeakPtr()));
  } else if (changing_gpm_pin_) {
    CHECK(model_->step() == Step::kGPMChangePin ||
          model_->step() == Step::kGPMChangeArbitraryPin);
    enclave_manager_->ChangePIN(
        base::UTF16ToUTF8(pin), std::move(*rapt_),
        base::BindOnce(&GPMEnclaveController::OnGpmPinChanged,
                       weak_ptr_factory_.GetWeakPtr()));
    rapt_.reset();
    ChangePinControllerImpl::RecordHistogram(ChangePinEvent::kNewPinEntered);
  } else {
    StartTransaction();
  }
}

void GPMEnclaveController::OnTouchIDComplete(bool success) {
  if (ShouldRefreshState()) {
    RefreshStateAndRepeatOperation();
    return;
  }
  // On error no LAContext will be provided and macOS will show the system UI
  // for user verification.
  model_->DisableUiOrShowLoadingDialog();
  StartTransaction();
}

void GPMEnclaveController::OnForgotGPMPinPressed() {
  changing_gpm_pin_ = true;
  model_->SetStep(Step::kGPMReauthForPinReset);
  ChangePinControllerImpl::RecordHistogram(
      ChangePinEvent::kFlowStartedFromPinDialog);
}

void GPMEnclaveController::OnReauthComplete(std::string rapt) {
  CHECK_EQ(model_->step(), Step::kGPMReauthForPinReset);
  rapt_ = std::move(rapt);
  model_->SetStep(Step::kGPMChangePin);
  ChangePinControllerImpl::RecordHistogram(ChangePinEvent::kReauthCompleted);
}

void GPMEnclaveController::StartTransaction() {
  // Starting a transaction means the user has chosen to use GPM. Reset the
  // decline count so GPM can again be the priority on creation.
  ResetDeclinedBootstrappingCount(GetProfile());
  pending_enclave_transaction_ = std::make_unique<GPMEnclaveTransaction>(
      /*delegate=*/this, PasskeyModelFactory::GetForProfile(GetProfile()),
      request_type_, rp_id_, enclave_manager_, pin_, selected_cred_id_,
      enclave_request_callback_);
  pending_enclave_transaction_->Start();
}

int GPMEnclaveController::GetFailedPINAttemptCount() {
  return GetProfile()->GetPrefs()->GetInteger(
      webauthn::pref_names::kEnclaveFailedPINAttemptsCount);
}

void GPMEnclaveController::SetFailedPINAttemptCount(int count) {
  GetProfile()->GetPrefs()->SetInteger(
      webauthn::pref_names::kEnclaveFailedPINAttemptsCount, count);
}

void GPMEnclaveController::HandlePINValidationResult(
    device::enclave::PINValidationResult result) {
  switch (result) {
    case device::enclave::PINValidationResult::kSuccess:
      SetFailedPINAttemptCount(0);
      model_->gpm_pin_remaining_attempts_ = std::nullopt;
      break;
    case device::enclave::PINValidationResult::kIncorrect: {
      int count = GetFailedPINAttemptCount();
      SetFailedPINAttemptCount(++count);

      if (count >= device::enclave::kMaxFailedPINAttempts) {
        model_->SetStep(Step::kGPMLockedPin);
      } else {
        model_->gpm_pin_remaining_attempts_ =
            device::enclave::kMaxFailedPINAttempts - count;
        PromptForPin();
      }
      break;
    }
    case device::enclave::PINValidationResult::kLocked:
      model_->SetStep(Step::kGPMLockedPin);
      break;
  }
}

bool GPMEnclaveController::BrowserIsApp() const {
  if (!web_contents()) {
    return false;
  }
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  return browser && browser->is_type_app();
}

void GPMEnclaveController::OnGpmPasskeysReset(bool success) {
  CHECK(model_->step() == Step::kRecoverSecurityDomain);
  if (!success ||
      model_->request_type != device::FidoRequestType::kMakeCredential) {
    model_->CancelAuthenticatorRequest();
    return;
  }
  // TODO(crbug.com/342554229): There might be a race between other members of
  // the domain. Maybe re-download the account state.
  SetAccountState(AccountState::kEmpty);
  model_->SetStep(AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
}

content::WebContents* GPMEnclaveController::web_contents() const {
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(render_frame_host_id_));
}
