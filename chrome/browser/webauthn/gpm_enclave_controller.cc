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
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
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

// ICloudMember holds a copyable subset of trusted_vault::VaultMember that we
// need for recovery.
struct GPMEnclaveController::ICloudMember {
  explicit ICloudMember(const trusted_vault::VaultMember& member)
      : public_key(member.public_key->ExportToBytes()) {
    for (const auto& member_key : member.member_keys) {
      if (member_key.version > version) {
        version = member_key.version;
        wrapped_key = member_key.wrapped_key;
      }
    }
  }

  // The result of exporting the SecureBoxPublicKey.
  std::vector<uint8_t> public_key;

  // The newest wrapped key for the member.
  std::vector<uint8_t> wrapped_key;

  // The key epoch.
  int version = 0;
};

// DownloadedAccountState holds the subset of information from
// `trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult` that is
// required for `GPMEnclaveController` to work. It exists because it's copyable,
// which the `trusted_vault` structure is not, and thus can be put in a cache.
struct GPMEnclaveController::DownloadedAccountState {
  explicit DownloadedAccountState(
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
          result,
      std::string gaia_id)
      : state(result.state),
        gpm_pin_metadata(std::move(result.gpm_pin_metadata)),
        lskf_expiries(std::move(result.lskf_expiries)),
        gaia_id(std::move(gaia_id)) {
    std::ranges::transform(result.icloud_keys, std::back_inserter(icloud_keys),
                           [](const trusted_vault::VaultMember& member) {
                             return ICloudMember(member);
                           });
  }

  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult::State
      state;
  std::optional<trusted_vault::GpmPinMetadata> gpm_pin_metadata;
  std::vector<ICloudMember> icloud_keys;
  std::vector<base::Time> lskf_expiries;
  std::string gaia_id;
};

// EnclaveUserVerificationMethod enumerates the possible ways that user
// verification will be performed for an enclave transaction.
enum class GPMEnclaveController::EnclaveUserVerificationMethod {
  // No user verification will be performed.
  kNone,
  // The user will enter a GPM PIN.
  kPIN,
  // User verification is satisfied because the user performed account recovery.
  kImplicit,
  // The operating system will perform user verification and allow signing
  // with the UV key.
  kUVKeyWithSystemUI,
  // The device is in a state waiting for an OS UV key to be created, which can
  // be done when a UV request is required.
  kDeferredUVKeyWithSystemUI,
  // Chrome will show user verification UI for the operating system, which will
  // then allow signing
  // with the UV key.
  kUVKeyWithChromeUI,
  // The request cannot be satisfied.
  kUnsatisfiable,
};

using EnclaveUserVerificationMethod =
    GPMEnclaveController::EnclaveUserVerificationMethod;

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

// AccountStateCache caches the account state between requests to reduce the
// load on the security domain service.
class AccountStateCache {
 public:
  std::optional<GPMEnclaveController::DownloadedAccountState> Get(
      base::Clock* clock) {
    if (!cache_time_) {
      return std::nullopt;
    }
    const base::Time now = clock->Now();
    if (now < *cache_time_ || (now - *cache_time_) > base::Minutes(30)) {
      cache_time_.reset();
      value_.reset();
      return std::nullopt;
    }
    return value_;
  }

  void Put(base::Clock* clock,
           const GPMEnclaveController::DownloadedAccountState& state) {
    if (base::FeatureList::IsEnabled(device::kWebAuthnCacheSecurityDomain)) {
      cache_time_ = clock->Now();
      value_ = state;
    }
  }

 private:
  std::optional<base::Time> cache_time_;
  std::optional<GPMEnclaveController::DownloadedAccountState> value_;
};

AccountStateCache* GetAccountStateCache() {
  static base::NoDestructor<AccountStateCache> cache;
  return cache.get();
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

void MaybeRecordUserActionForWinUv(bool is_create,
                                   EnclaveUserVerificationMethod uv_method) {
#if BUILDFLAG(IS_WIN)
  if (uv_method == EnclaveUserVerificationMethod::kUVKeyWithSystemUI ||
      uv_method == EnclaveUserVerificationMethod::kDeferredUVKeyWithSystemUI) {
    webauthn::user_actions::RecordGpmWinUvShown(is_create);
  }
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace

GPMEnclaveController::GPMEnclaveController(
    content::RenderFrameHost* render_frame_host,
    AuthenticatorRequestDialogModel* model,
    const std::string& rp_id,
    device::FidoRequestType request_type,
    device::UserVerificationRequirement user_verification_requirement,
    base::Clock* clock,
    std::unique_ptr<trusted_vault::TrustedVaultConnection> optional_connection)
    : render_frame_host_id_(render_frame_host->GetGlobalId()),
      rp_id_(rp_id),
      request_type_(request_type),
      user_verification_requirement_(user_verification_requirement),
      enclave_manager_(EnclaveManagerFactory::GetAsEnclaveManagerForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))),
      model_(model),
      vault_connection_override_(std::move(optional_connection)),
      clock_(clock) {
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
    SetActive(false);
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
    model_->EnclaveNeedsReauth();
    SetActive(false);
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
          SetActive(true);
          return;
      }
    }

    if (device::kWebAuthnGpmPin.Get()) {
      // For get() requests, progress the UI now because, with GPM PIN support,
      // we can handle the account in any state and we'll block the UI if needed
      // when the user selects a GPM credential.
      SetActive(true);
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
    SetActive(false);
    return;
  }

  DownloadAccountState();
}

void GPMEnclaveController::DownloadAccountState() {
  std::optional<DownloadedAccountState> maybe_cached;
  // If the enclave_manager isn't ready then a cached account state can be used
  // to reduce load on the security domain service. If it is ready then this
  // must be a create() request, and we want to check that the security domain
  // epoch hasn't changed and so don't use a cached state.
  if (!enclave_manager_->is_ready()) {
    // TODO(enclave): discard cache if gaia id no longer matches.
    maybe_cached = GetAccountStateCache()->Get(clock_);
  }
  if (maybe_cached) {
    FIDO_LOG(EVENT) << "Using cached account state";
    OnHaveAccountState(std::move(*maybe_cached));
    return;
  }

  FIDO_LOG(EVENT) << "Fetching account state";
  account_state_ = AccountState::kChecking;

  account_state_timeout_ = std::make_unique<base::OneShotTimer>();
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
                         std::move(trusted_vault_conn)));
}

void GPMEnclaveController::OnAccountStateTimeOut() {
  FIDO_LOG(ERROR) << "Fetching the account state timed out.";
  download_account_state_request_.reset();
  if (enclave_manager_->is_ready()) {
    // If we were checking the security domain just to check whether the epoch
    // has changed then we assume that it hasn't.
    SetAccountStateReady();
    SetActive(true);
  } else {
    model_->OnLoadingEnclaveTimeout();
    account_state_ = AccountState::kNone;
    SetActive(false);
  }
}

void GPMEnclaveController::OnAccountStateDownloaded(
    std::string gaia_id,
    std::unique_ptr<trusted_vault::TrustedVaultConnection> unused,
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        result) {
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
    SetActive(true);
    return;
  }

  DownloadedAccountState downloaded(std::move(result), std::move(gaia_id));
  GetAccountStateCache()->Put(clock_, downloaded);

  OnHaveAccountState(DownloadedAccountState(std::move(downloaded)));
}

void GPMEnclaveController::OnHaveAccountState(DownloadedAccountState result) {
  using Result =
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;
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
  user_gaia_id_ = std::move(result.gaia_id);

  if (device::kWebAuthnGpmPin.Get()) {
    SetActive(account_state_ != AccountState::kNone);
  } else {
    SetActive(account_state_ == AccountState::kRecoverable);
  }
}

void GPMEnclaveController::SetActive(bool active) {
  is_active_ = active;
  if (waiting_for_account_state_) {
    std::move(waiting_for_account_state_).Run();
  }
  if (ready_for_ui_) {
    return;
  }
  ready_for_ui_ = true;
  if (active) {
    model_->EnclaveEnabled();
  }
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
  for (const GPMEnclaveController::ICloudMember& recovery_icloud_key :
       security_domain_icloud_recovery_keys_) {
    const auto local_icloud_key_it = std::ranges::find_if(
        local_icloud_keys, [&recovery_icloud_key](const auto& key) {
          return key->id() == recovery_icloud_key.public_key;
        });
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
        local_icloud_key_it = std::ranges::find_if(
            local_icloud_keys, [&recovery_icloud_key](const auto& key) {
              return key->id() == recovery_icloud_key.public_key;
            });
        return local_icloud_key_it != local_icloud_keys.end();
      });
  if (local_icloud_key_it == local_icloud_keys.end()) {
    FIDO_LOG(DEBUG) << "Could not find matching iCloud recovery key";
    model_->SetStep(Step::kRecoverSecurityDomain);
    return;
  }
  std::optional<std::vector<uint8_t>> security_domain_secret =
      trusted_vault::DecryptTrustedVaultWrappedKey(
          (*local_icloud_key_it)->key()->private_key(),
          recovery_icloud_key_it->wrapped_key);
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
                              recovery_icloud_key_it->version);
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
          if (model_->step() != Step::kConditionalMediation) {
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
      if (model_->step() != Step::kConditionalMediation &&
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
  access_token_fetcher_ = enclave_manager_->GetAccessToken(base::BindOnce(
      &GPMEnclaveController::MaybeHashPinAndStartEnclaveTransaction,
      weak_ptr_factory_.GetWeakPtr()));
}

void GPMEnclaveController::MaybeHashPinAndStartEnclaveTransaction(
    std::optional<std::string> token) {
  if (!pin_) {
    StartEnclaveTransaction(std::move(token), nullptr);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(
          [](std::string pin,
             std::unique_ptr<webauthn_pb::EnclaveLocalState_WrappedPIN>
                 wrapped_pin) -> std::unique_ptr<device::enclave::ClaimedPIN> {
            return EnclaveManager::MakeClaimedPINSlowly(std::move(pin),
                                                        std::move(wrapped_pin));
          },
          *pin_, enclave_manager_->GetWrappedPIN()),
      base::BindOnce(&GPMEnclaveController::StartEnclaveTransaction,
                     weak_ptr_factory_.GetWeakPtr(), std::move(token)));
}

void GPMEnclaveController::StartEnclaveTransaction(
    std::optional<std::string> token,
    std::unique_ptr<device::enclave::ClaimedPIN> claimed_pin) {
  // The UI has advanced to the point where it wants to perform an enclave
  // transaction. This code collects the needed values and triggers
  // `enclave_request_callback_` which surfaces in
  // `EnclaveDiscovery::OnUIRequest`.

  if (!token || !enclave_manager_->is_ready()) {
    model_->SetStep(Step::kGPMError);
    return;
  }

  auto request = std::make_unique<device::enclave::CredentialRequest>();
  request->access_token = std::move(*token);
  // A request to the enclave can either provide a wrapped secret, which only
  // the enclave can decrypt, or can provide the security domain secret
  // directly. The latter is only possible immediately after registering a
  // device because that's the only time that the actual security domain secret
  // is in memory.
  bool use_unwrapped_secret = false;

  switch (*uv_method_) {
    case EnclaveUserVerificationMethod::kNone:
      request->signing_callback =
          enclave_manager_->IdentityKeySigningCallback();
      break;

    case EnclaveUserVerificationMethod::kImplicit:
      request->signing_callback =
          enclave_manager_->IdentityKeySigningCallback();
      use_unwrapped_secret = true;
      request->user_verified = true;
      break;

    case EnclaveUserVerificationMethod::kPIN:
      request->signing_callback =
          enclave_manager_->IdentityKeySigningCallback();
      CHECK(claimed_pin);
      request->claimed_pin = std::move(claimed_pin);
      request->pin_result_callback =
          base::BindOnce(&GPMEnclaveController::HandlePINValidationResult,
                         weak_ptr_factory_.GetWeakPtr());
      request->user_verified = true;
      break;

    case EnclaveUserVerificationMethod::kUVKeyWithChromeUI:
    case EnclaveUserVerificationMethod::kUVKeyWithSystemUI: {
      EnclaveManager::UVKeyOptions uv_options;
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
      request->signing_callback =
          enclave_manager_->UserVerifyingKeySigningCallback(
              std::move(uv_options));
      request->user_verified = true;
      MaybeRecordUserActionForWinUv(
          request_type_ == device::FidoRequestType::kMakeCredential,
          uv_method_.value());
      break;
    }
    case EnclaveUserVerificationMethod::kDeferredUVKeyWithSystemUI:
      // This submits a UV key, but is signed with the HW key. We still count
      // it as being user verified because this will trigger UV creation and
      // the system will verify the user for that operation.
      request->signing_callback =
          enclave_manager_->IdentityKeySigningCallback();
      request->user_verified = true;
      request->uv_key_creation_callback =
          enclave_manager_->UserVerifyingKeyCreationCallback();
      request->unregister_callback =
          base::BindOnce(&EnclaveManager::Unenroll,
                         enclave_manager_->GetWeakPtr(), base::DoNothing());
      MaybeRecordUserActionForWinUv(
          request_type_ == device::FidoRequestType::kMakeCredential,
          uv_method_.value());
      break;
    case EnclaveUserVerificationMethod::kUnsatisfiable:
      NOTREACHED();
  }

  switch (request_type_) {
    case device::FidoRequestType::kMakeCredential: {
      if (use_unwrapped_secret) {
        std::tie(request->key_version, request->secret) =
            enclave_manager_->TakeSecret().value();
      } else {
        std::tie(request->key_version, request->wrapped_secret) =
            enclave_manager_->GetCurrentWrappedSecret();
      }
      request->save_passkey_callback =
          base::BindOnce(&GPMEnclaveController::OnPasskeyCreated,
                         weak_ptr_factory_.GetWeakPtr());
      base::ranges::transform(
          creds_, std::back_inserter(request->existing_cred_ids),
          [](const auto& cred) {
            const std::string& cred_id = cred.credential_id();
            return std::vector<uint8_t>(cred_id.begin(), cred_id.end());
          });

      break;
    }

    case device::FidoRequestType::kGetAssertion: {
      std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity;
      for (const auto& cred : creds_) {
        if (base::ranges::equal(
                base::as_bytes(base::make_span(cred.credential_id())),
                base::make_span(*selected_cred_id_))) {
          entity = std::make_unique<sync_pb::WebauthnCredentialSpecifics>(cred);
          break;
        }
      }
      CHECK(entity);

      if (use_unwrapped_secret) {
        std::tie(std::ignore, request->secret) =
            enclave_manager_->TakeSecret().value();
      } else {
        if (entity->key_version()) {
          std::optional<std::vector<uint8_t>> wrapped_secret =
              enclave_manager_->GetWrappedSecret(entity->key_version());
          if (wrapped_secret) {
            request->wrapped_secret = std::move(*wrapped_secret);
          } else {
            FIDO_LOG(ERROR)
                << "Unexpectedly did not have a wrapped key for epoch "
                << entity->key_version();
          }
        }
        if (!request->wrapped_secret.has_value()) {
          request->wrapped_secret =
              enclave_manager_->GetCurrentWrappedSecret().second;
        }
      }

      request->entity = std::move(entity);
      break;
    }
  }

  CHECK(request->wrapped_secret.has_value() ^ request->secret.has_value());
  enclave_request_callback_.Run(std::move(request));
}

void GPMEnclaveController::OnPasskeyCreated(
    sync_pb::WebauthnCredentialSpecifics passkey) {
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetInstance()->GetForProfile(GetProfile());
  passkey_model->CreatePasskey(passkey);

  if (device::kWebAuthnGpmPin.Get()) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(
            content::RenderFrameHost::FromID(render_frame_host_id_));

    PasswordsClientUIDelegate* manage_passwords_ui_controller =
        PasswordsClientUIDelegateFromWebContents(web_contents);
    if (manage_passwords_ui_controller) {
      bool gpm_pin_created_in_this_request =
          gpm_pin_creation_confirmed_ && enclave_manager_->has_wrapped_pin();
      manage_passwords_ui_controller->OnPasskeySaved(
          gpm_pin_created_in_this_request);
    }
  }
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
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_frame_host_id_));
  if (!web_contents) {
    return false;
  }
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
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
