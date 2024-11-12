// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm_enclave_controller.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
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
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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
#include "chrome/browser/webauthn/gpm_user_verification_policy.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
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
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/webauthn_dialog_controller.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/common/chrome_version.h"
#include "device/fido/enclave/icloud_recovery_key_mac.h"
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
#if BUILDFLAG(IS_MAC)
  constexpr bool kIsMac = true;
#else
  constexpr bool kIsMac = false;
#endif

  if (have_entered_pin_for_recovery) {
    return EnclaveUserVerificationMethod::kImplicit;
  }

  // If the platform has biometrics now, but didn't when we enrolled, we need to
  // act as if they are missing because we've no UV key to use them with.
  if (uv_key_state == EnclaveManager::UvKeyState::kNone) {
    platform_has_biometrics = false;
  }

  if (!GpmWillDoUserVerification(uv, platform_has_biometrics)) {
    return EnclaveUserVerificationMethod::kNone;
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

bool ExpiryTooSoon(base::Time expiry) {
  const base::Time now = base::Time::Now();
  // LSKFs must have at least 18 weeks of validity on them because we don't want
  // users depending on an LSKF from a device that they've stopped using.
  // Validities are generally six months, thus this implies that the device was
  // used in the previous six weeks.
  return expiry < now || (expiry - now) < base::Days(7 * 18);
}

void ResetDeclinedBootstrappingCount(
    content::RenderFrameHost* render_frame_host) {
  Profile::FromBrowserContext(render_frame_host->GetBrowserContext())
      ->GetPrefs()
      ->SetInteger(webauthn::pref_names::kEnclaveDeclinedGPMBootstrappingCount,
                   0);
}

}  // namespace

GPMEnclaveController::GPMEnclaveController(
    content::RenderFrameHost* render_frame_host,
    AuthenticatorRequestDialogModel* model,
    const std::string& rp_id,
    device::FidoRequestType request_type,
    device::UserVerificationRequirement user_verification_requirement,
    base::TickClock const* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<trusted_vault::TrustedVaultConnection> optional_connection)
    : render_frame_host_id_(render_frame_host->GetGlobalId()),
      rp_id_(rp_id),
      request_type_(request_type),
      user_verification_requirement_(user_verification_requirement),
      enclave_manager_(EnclaveManagerFactory::GetAsEnclaveManagerForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))),
      model_(model),
      vault_connection_override_(std::move(optional_connection)),
      tick_clock_(tick_clock),
      timer_task_runner_(std::move(task_runner)) {
  enclave_manager_observer_.Observe(enclave_manager_);
  model_observer_.Observe(model_);

  Profile* const profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetInstance()->GetForProfile(profile);
  creds_ = passkey_model->GetPasskeysForRelyingPartyId(rp_id_);

  // The following code may do some asynchronous processing. However the control
  // flow terminates, it must have:
  //   a) set `account_state_` to some value (unless it's happy with the default
  //      of `kNone`.)
  //   b) called `SetActive`.
  if (creds_.empty() &&
      request_type == device::FidoRequestType::kGetAssertion) {
    // No possibility of using GPM for this request.
    FIDO_LOG(EVENT) << "Enclave is not a candidate for this request";
    SetActive(EnclaveEnabledStatus::kDisabled);
  } else if (enclave_manager_->is_loaded()) {
    OnEnclaveLoaded();
  } else {
    FIDO_LOG(EVENT) << "Loading enclave state";
    account_state_ = AccountState::kLoading;
    enclave_manager_->Load(
        base::BindOnce(&GPMEnclaveController::OnEnclaveLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

GPMEnclaveController::~GPMEnclaveController() {
  // Ensure that any secret is dropped from memory after a transaction.
  enclave_manager_->TakeSecret();
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
#if BUILDFLAG(IS_MAC)
  uv_options.lacontext = std::move(model_->lacontext);
#endif  // BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::features::IsWebAuthNAuthDialogMergeEnabled()) {
    uv_options.dialog_controller = ash::ActiveSessionAuthController::Get();
  } else {
    uv_options.dialog_controller = ash::WebAuthNDialogController::Get();
  }
#endif
}

void GPMEnclaveController::OnPasskeyCreated(
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
  if (!device::kWebAuthnGpmPin.Get()) {
    return;
  }
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  if (manage_passwords_ui_controller) {
    bool gpm_pin_created_in_this_request =
        gpm_pin_creation_confirmed_ && enclave_manager_->has_wrapped_pin();
    manage_passwords_ui_controller->OnPasskeySaved(
        gpm_pin_created_in_this_request, rp_id_);
  }
}

Profile* GPMEnclaveController::GetProfile() const {
  return Profile::FromBrowserContext(
             content::RenderFrameHost::FromID(render_frame_host_id_)
                 ->GetBrowserContext())
      ->GetOriginalProfile();
}

GPMEnclaveController::AccountState
GPMEnclaveController::account_state_for_testing() const {
  return account_state_;
}

void GPMEnclaveController::OnEnclaveLoaded() {
  // Verify the state of the primary account sign-in info.
  auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  GoogleServiceAuthError signin_error =
      identity_manager->GetErrorStateOfRefreshTokenForAccount(
          account.account_id);
  if (signin_error.IsPersistentError()) {
    FIDO_LOG(EVENT) << "Recoverable sign-in error: " << signin_error.ToString();
    account_state_ = AccountState::kNone;
    SetActive(EnclaveEnabledStatus::kEnabledAndReauthNeeded);
    return;
  }

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
          SetAccountStateReady();
          SetActive(EnclaveEnabledStatus::kEnabled);
          return;
      }
    }

    if (device::kWebAuthnGpmPin.Get()) {
      // For get() requests, progress the UI now because, with GPM PIN support,
      // we can handle the account in any state and we'll block the UI if needed
      // when the user selects a GPM credential.
      SetActive(EnclaveEnabledStatus::kEnabled);
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

  if (!can_make_uv_keys && !device::kWebAuthnGpmPin.Get()) {
    // Without the ability to do user verification, we cannot enroll the current
    // device.
    account_state_ = AccountState::kNone;
    SetActive(EnclaveEnabledStatus::kDisabled);
    return;
  }

  DownloadAccountState();
}

void GPMEnclaveController::DownloadAccountState() {
  FIDO_LOG(EVENT) << "Fetching account state";
  account_state_ = AccountState::kChecking;

  account_state_timeout_ = std::make_unique<base::OneShotTimer>(tick_clock_);
  if (timer_task_runner_) {
    account_state_timeout_->SetTaskRunner(timer_task_runner_);
  }
  account_state_timeout_->Start(
      FROM_HERE, kDownloadAccountStateTimeout,
      base::BindOnce(&GPMEnclaveController::OnAccountStateTimeOut,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  scoped_refptr<network::SharedURLLoaderFactory> testing_url_loader =
      EnclaveManagerFactory::url_loader_override();
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      testing_url_loader ? testing_url_loader
                         : SystemNetworkContextManager::GetInstance()
                               ->GetSharedURLLoaderFactory();
  std::unique_ptr<trusted_vault::TrustedVaultConnection> trusted_vault_conn =
      vault_connection_override_
          ? std::move(vault_connection_override_)
          : trusted_vault::NewFrontendTrustedVaultConnection(
                trusted_vault::SecurityDomainId::kPasskeys, identity_manager,
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
          base::BindRepeating(&GPMEnclaveController::OnAccountStateKeepAlive,
                              weak_ptr_factory_.GetWeakPtr()));
}

void GPMEnclaveController::OnAccountStateKeepAlive() {
  account_state_timeout_->Reset();
}

void GPMEnclaveController::OnAccountStateTimeOut() {
  FIDO_LOG(ERROR) << "Fetching the account state timed out.";
  device::enclave::RecordEvent(
      device::enclave::Event::kDownloadAccountStateTimeout);
  download_account_state_request_.reset();
  if (enclave_manager_->is_ready()) {
    // If we were checking the security domain just to check whether the epoch
    // has changed then we assume that it hasn't.
    SetAccountStateReady();
    SetActive(EnclaveEnabledStatus::kEnabled);
  } else {
    model_->OnLoadingEnclaveTimeout();
    account_state_ = AccountState::kNone;
    SetActive(EnclaveEnabledStatus::kDisabled);
  }
}

void GPMEnclaveController::OnAccountStateDownloaded(
    std::string gaia_id,
    std::unique_ptr<trusted_vault::TrustedVaultConnection> unused,
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        result) {
  using Result =
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;
  if (account_state_ != AccountState::kChecking) {
    // This request timed out.
    return;
  }
  download_account_state_request_.reset();
  account_state_timeout_.reset();

  FIDO_LOG(EVENT) << "Download account state result: " << ToString(result.state)
                  << ", key_version: " << result.key_version.value_or(0)
                  << ", has PIN: " << result.gpm_pin_metadata.has_value()
                  << ", expiry: "
                  << (result.gpm_pin_metadata.has_value()
                          ? base::TimeFormatAsIso8601(
                                result.gpm_pin_metadata->expiry)
                          : "<none>")
                  << ", iCloud Keychain keys: " << result.icloud_keys.size();

  if (enclave_manager_->is_ready() &&
      enclave_manager_->ConsiderSecurityDomainState(result,
                                                    base::DoNothing())) {
    SetAccountStateReady();
    SetActive(EnclaveEnabledStatus::kEnabled);
    return;
  }

  FIDO_LOG(EVENT) << "Account state: " << ToString(result.state)
                  << ", has PIN: " << result.gpm_pin_metadata.has_value()
                  << ", iCloud Keychain keys: " << result.icloud_keys.size();

  if (!device::kWebAuthnGpmPin.Get() &&
      result.state == Result::State::kRecoverable &&
      !result.lskf_expiries.empty() &&
      base::ranges::all_of(result.lskf_expiries, ExpiryTooSoon)) {
    std::vector<std::string> expiries;
    base::ranges::transform(
        result.lskf_expiries, std::back_inserter(expiries),
        [](const auto& time) { return base::TimeFormatAsIso8601(time); });
    FIDO_LOG(EVENT) << "Account considered irrecoverable because no LSKF has "
                       "acceptable expiry: "
                    << base::JoinString(expiries, ", ");
    result.state = Result::State::kIrrecoverable;
  }

  switch (result.state) {
    case Result::State::kError:
      account_state_ = AccountState::kNone;
      break;

    case Result::State::kEmpty:
      account_state_ = AccountState::kEmpty;
      break;

    case Result::State::kRecoverable:
      account_state_ = AccountState::kRecoverable;
      break;

    case Result::State::kIrrecoverable:
      account_state_ = AccountState::kIrrecoverable;
      break;
  }

  if (result.gpm_pin_metadata) {
    pin_metadata_ = std::move(result.gpm_pin_metadata);
  }
  security_domain_icloud_recovery_keys_ = std::move(result.icloud_keys);
  user_gaia_id_ = std::move(gaia_id);

  if (device::kWebAuthnGpmPin.Get()) {
    SetActive(account_state_ != AccountState::kNone
                  ? EnclaveEnabledStatus::kEnabled
                  : EnclaveEnabledStatus::kDisabled);
  } else {
    SetActive(account_state_ == AccountState::kRecoverable
                  ? EnclaveEnabledStatus::kEnabled
                  : EnclaveEnabledStatus::kDisabled);
  }
}

void GPMEnclaveController::SetActive(EnclaveEnabledStatus status) {
  is_active_ = status == EnclaveEnabledStatus::kEnabled;
  if (waiting_for_account_state_) {
    std::move(waiting_for_account_state_).Run();
  }
  if (ready_for_ui_) {
    return;
  }
  ready_for_ui_ = true;
  model_->EnclaveEnabledStatusChanged(status);
  model_->OnReadyForUI();
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
  } else {
    // Keys were stored but we were not expecting it, e.g. because it happened
    // during a request at a different step on another tab. Ignore it.
    return;
  }

  CHECK(enclave_manager_->has_pending_keys());
  CHECK(!enclave_manager_->is_ready());

  if (pin_metadata_.has_value() || *can_make_uv_keys_) {
    if (!enclave_manager_->AddDeviceToAccount(
            std::move(pin_metadata_),
            base::BindOnce(&GPMEnclaveController::OnDeviceAdded,
                           weak_ptr_factory_.GetWeakPtr()))) {
      model_->SetStep(Step::kGPMError);
    }
  } else {
    // Create a GPM PIN if the user doesn't have one and can't make
    // a UV key locally.
    model_->SetStep(Step::kGPMCreatePin);
  }
}

void GPMEnclaveController::OnDeviceAdded(bool success) {
  ResetDeclinedBootstrappingCount(
      content::RenderFrameHost::FromID(render_frame_host_id_));
  if (!success) {
    model_->SetStep(Step::kGPMError);
    return;
  }

#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(device::kWebAuthnICloudRecoveryKey)) {
    MaybeAddICloudRecoveryKey();
    return;
  }
#endif

  OnEnclaveAccountSetUpComplete();
}

void GPMEnclaveController::RecoverSecurityDomain() {
#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(
          device::kWebAuthnRecoverFromICloudRecoveryKey)) {
    model_->DisableUiOrShowLoadingDialog();
    device::enclave::ICloudRecoveryKey::Retrieve(
        base::BindOnce(&GPMEnclaveController::OnICloudKeysRetrievedForRecovery,
                       weak_ptr_factory_.GetWeakPtr()),
        kICloudKeychainRecoveryKeyAccessGroup);
  } else {
    model_->SetStep(Step::kRecoverSecurityDomain);
  }
#else
  model_->SetStep(Step::kRecoverSecurityDomain);
#endif  // BUILDFLAG(IS_MAC)
}

#if BUILDFLAG(IS_MAC)

void GPMEnclaveController::MaybeAddICloudRecoveryKey() {
  device::enclave::ICloudRecoveryKey::Retrieve(
      base::BindOnce(&GPMEnclaveController::OnICloudKeysRetrievedForEnrollment,
                     weak_ptr_factory_.GetWeakPtr()),
      kICloudKeychainRecoveryKeyAccessGroup);
}

void GPMEnclaveController::OnICloudKeysRetrievedForEnrollment(
    std::vector<std::unique_ptr<device::enclave::ICloudRecoveryKey>>
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
  device::enclave::ICloudRecoveryKey::Create(
      base::BindOnce(&GPMEnclaveController::EnrollICloudRecoveryKey,
                     weak_ptr_factory_.GetWeakPtr()),
      kICloudKeychainRecoveryKeyAccessGroup);
}

void GPMEnclaveController::EnrollICloudRecoveryKey(
    std::unique_ptr<device::enclave::ICloudRecoveryKey> key) {
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
    std::vector<std::unique_ptr<device::enclave::ICloudRecoveryKey>>
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
    model_->SetStep(Step::kRecoverSecurityDomain);
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
    model_->SetStep(Step::kRecoverSecurityDomain);
    return;
  }
  FIDO_LOG(EVENT) << "Successful recovery from iCloud recovery key";
  recovered_with_icloud_keychain_ = true;
  enclave_manager_->StoreKeys(user_gaia_id_,
                              {std::move(*security_domain_secret)},
                              member_key_it->version);
}

#endif  // BUILDFLAG(IS_MAC)

void GPMEnclaveController::OnEnclaveAccountSetUpComplete() {
  have_added_device_ = true;
  SetAccountStateReady();
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
    case EnclaveUserVerificationMethod::kNone:
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
  }
}

void GPMEnclaveController::SetAccountStateReady() {
  account_state_ = AccountState::kReady;
  pin_is_arbitrary_ = enclave_manager_->has_wrapped_pin() &&
                      enclave_manager_->wrapped_pin_is_arbitrary();
}

void GPMEnclaveController::OnGPMSelected() {
  if (model_->is_off_the_record && !off_the_record_confirmed_) {
    model_->SetStep(Step::kGPMConfirmOffTheRecordCreate);
    return;
  }

  switch (account_state_) {
    case AccountState::kEmpty:
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
        case EnclaveUserVerificationMethod::kNone:
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
      }
      break;

    case AccountState::kRecoverable:
    case AccountState::kIrrecoverable:
      device::enclave::RecordEvent(device::enclave::Event::kOnboarding);
      model_->SetStep(Step::kTrustThisComputerCreation);
      break;

    case AccountState::kLoading:
    case AccountState::kChecking:
      waiting_for_account_state_ = base::BindOnce(
          &GPMEnclaveController::OnGPMSelected, weak_ptr_factory_.GetWeakPtr());
      // TODO(rgod): If the model step is `kNotStarted`, no UI is visible yet.
      // Display a loading dialog after a delay, so it doesn't flicker in case
      // the account state is fetched quickly.
      model_->DisableUiOrShowLoadingDialog();
      break;

    case AccountState::kNone:
      model_->SetStep(Step::kGPMError);
      break;
  }
}

void GPMEnclaveController::OnGPMPasskeySelected(
    std::vector<uint8_t> credential_id) {
  selected_cred_id_ = std::move(credential_id);
  // Change the Step from `kPasskeyAutofill` so that it's clear that an
  // operation is in progress. This was originally motivated because updating
  // the "last used" field in a passkey entity triggered a callback that
  // restarted the request because the Step hadn't been updated.
  if (model_->step() == Step::kPasskeyAutofill &&
      base::FeatureList::IsEnabled(device::kWebAuthnUpdateLastUsed)) {
    model_->SetStep(Step::kNotStarted);
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
        case EnclaveUserVerificationMethod::kNone:
        case EnclaveUserVerificationMethod::kImplicit:
          if (model_->step() != Step::kPasskeyAutofill) {
            // The autofill UI shows its own loading indicator.
            model_->DisableUiOrShowLoadingDialog();
          }
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
      }
      break;

    case AccountState::kRecoverable:
    case AccountState::kIrrecoverable:
      if (model_->priority_phone_name.has_value()) {
        device::enclave::RecordEvent(device::enclave::Event::kOnboarding);
        model_->SetStep(Step::kTrustThisComputerAssertion);
      } else {
        RecoverSecurityDomain();
      }
      break;

    case AccountState::kLoading:
    case AccountState::kChecking:
      if (model_->step() != Step::kPasskeyAutofill &&
          model_->step() != Step::kNotStarted) {
        // The autofill UI shows its own loading indicator.
        model_->DisableUiOrShowLoadingDialog();
      }
      waiting_for_account_state_ =
          base::BindOnce(&GPMEnclaveController::OnGPMPasskeySelected,
                         weak_ptr_factory_.GetWeakPtr(), *selected_cred_id_);
      break;

    case AccountState::kNone:
      if (model_->priority_phone_name.has_value()) {
        model_->ContactPriorityPhone();
      } else {
        // This can happen if a passkey is selected after the enclave times out.
        model_->SetStep(Step::kGPMError);
      }
      break;

    case AccountState::kEmpty:
      if (model_->priority_phone_name.has_value()) {
        model_->ContactPriorityPhone();
      } else {
        // The security domain is empty but there were
        // sync entities. Most like the security domain was reset without
        // clearing the entities, thus they are unusable.
        model_->SetStep(Step::kGPMError);
      }
      break;
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

void GPMEnclaveController::OnTrustThisComputer() {
  CHECK(model_->step() == Step::kTrustThisComputerAssertion ||
        model_->step() == Step::kTrustThisComputerCreation);
  device::enclave::RecordEvent(device::enclave::Event::kOnboardingAccepted);
  // Clicking through the bootstrapping dialog resets the count even if it
  // doesn't end up being successful.
  ResetDeclinedBootstrappingCount(
      content::RenderFrameHost::FromID(render_frame_host_id_));
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
  CHECK_EQ(model_->step(), Step::kGPMCreatePasskey);
  CHECK(account_state_ == AccountState::kEmpty ||
        account_state_ == AccountState::kReady);
  if (account_state_ == AccountState::kEmpty) {
    model_->SetStep(Step::kGPMCreatePin);
  } else {
    switch (*uv_method_) {
      case EnclaveUserVerificationMethod::kUVKeyWithSystemUI:
      case EnclaveUserVerificationMethod::kDeferredUVKeyWithSystemUI:
      case EnclaveUserVerificationMethod::kNone:
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
        *pin_, base::BindOnce(&GPMEnclaveController::OnDeviceAdded,
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
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_frame_host_id_);
  Profile::FromBrowserContext(rfh->GetBrowserContext())
      ->GetPrefs()
      ->SetInteger(
          webauthn::pref_names::kEnclaveDeclinedGPMCredentialCreationCount, 0);
  pending_enclave_transaction_ = std::make_unique<GPMEnclaveTransaction>(
      /*delegate=*/this, PasskeyModelFactory::GetForProfile(GetProfile()),
      request_type_, rp_id_, *uv_method_, enclave_manager_, pin_,
      selected_cred_id_, enclave_request_callback_);
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
      model_->request_type != device::FidoRequestType::kMakeCredential ||
      !device::kWebAuthnGpmPin.Get()) {
    model_->CancelAuthenticatorRequest();
    return;
  }
  // TODO(crbug.com/342554229): There might be a race between other members of
  // the domain. Maybe re-download the account state.
  account_state_ = AccountState::kEmpty;
  model_->SetStep(AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
}

content::WebContents* GPMEnclaveController::web_contents() const {
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(render_frame_host_id_));
}
