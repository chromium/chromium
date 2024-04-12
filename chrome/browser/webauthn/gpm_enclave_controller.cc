// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm_enclave_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
#include "components/device_event_log/device_event_log.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/trusted_vault/frontend_trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_types.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using Step = AuthenticatorRequestDialogModel::Step;

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
//   OnGPMSelected -> kGPMOnboarding -> OnGPMOnboardingAccepted ->
//     kGPMCreatePin -> OnGPMPinEntered -> OnDeviceAdded
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
// |     kGPMOnboarding      |
// +-------------------------+
//   |
//   |
//   v
// +-------------------------+
// | OnGPMOnboardingAccepted |
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

// EnclaveUserVerificationMethod enumerates the possible ways that user
// verification will be performed for an enclave transaction.
enum class EnclaveUserVerificationMethod {
  // No user verification will be performed.
  kNone,
  // The user will enter a GPM PIN.
  kPIN,
  // User verification is satisfied because the user performed account recovery.
  kImplicit,
  // The operating system will perform user verification and allow signing
  // with the UV key.
  kUVKeyWithSystemUI,
  // Chrome will show user verification UI for the operating system, which will
  // then allow signing
  // with the UV key.
  kUVKeyWithChromeUI,
  // The request cannot be satisfied.
  kUnsatisfiable,
};

// Pick an enclave user verification method for a specific request.
EnclaveUserVerificationMethod PickEnclaveUserVerificationMethod(
    device::UserVerificationRequirement uv,
    bool have_added_device,
    bool has_pin,
    EnclaveManager::UvKeyState uv_key_state) {
  switch (uv) {
    case device::UserVerificationRequirement::kDiscouraged:
      return EnclaveUserVerificationMethod::kNone;

    case device::UserVerificationRequirement::kPreferred:
    case device::UserVerificationRequirement::kRequired:
      switch (uv_key_state) {
        case EnclaveManager::UvKeyState::kNone:
          if (have_added_device) {
            return EnclaveUserVerificationMethod::kImplicit;
          } else if (has_pin) {
            return EnclaveUserVerificationMethod::kPIN;
          } else if (uv == device::UserVerificationRequirement::kPreferred) {
            return EnclaveUserVerificationMethod::kNone;
          } else {
            return EnclaveUserVerificationMethod::kUnsatisfiable;
          }

        case EnclaveManager::UvKeyState::kUsesSystemUI:
          return EnclaveUserVerificationMethod::kUVKeyWithSystemUI;

        case EnclaveManager::UvKeyState::kUsesChromeUI:
          return EnclaveUserVerificationMethod::kUVKeyWithChromeUI;
      }
  }
}

}  // namespace

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
      enclave_manager_(EnclaveManagerFactory::GetForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))),
      model_(model) {
  enclave_manager_observer_.Observe(enclave_manager_);
  model_observer_.Observe(&model_->observers);

  Profile* const profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  webauthn::PasskeyModel* passkey_model =
      PasskeyModelFactory::GetInstance()->GetForProfile(profile);
  creds_ = passkey_model->GetPasskeysForRelyingPartyId(rp_id_);

  if (creds_.empty() &&
      request_type == device::FidoRequestType::kGetAssertion) {
    // No possibility of using GPM for this request.
    FIDO_LOG(EVENT) << "Enclave is not a candidate for this request";
  } else if (enclave_manager_->is_ready()) {
    FIDO_LOG(EVENT) << "Enclave is ready";
    SetAccountStateReady();
  } else if (enclave_manager_->is_loaded()) {
    FIDO_LOG(EVENT) << "Account state needs to be checked";
    account_state_ = AccountState::kChecking;
    DownloadAccountState(profile);
  } else {
    FIDO_LOG(EVENT) << "Enclave state is loading";
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

bool GPMEnclaveController::ready_for_ui() const {
  return account_state_ != AccountState::kLoading;
}

void GPMEnclaveController::ConfigureDiscoveries(
    device::FidoDiscoveryFactory* discovery_factory) {
  discovery_factory->set_enclave_passkey_creation_callback(base::BindRepeating(
      &GPMEnclaveController::OnPasskeyCreated, weak_ptr_factory_.GetWeakPtr()));

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

void GPMEnclaveController::SetTrustedVaultConnectionForTesting(
    std::unique_ptr<trusted_vault::TrustedVaultConnection> connection) {
  vault_connection_override_ = std::move(connection);
}

Profile* GPMEnclaveController::GetProfile() const {
  return Profile::FromBrowserContext(
      content::RenderFrameHost::FromID(render_frame_host_id_)
          ->GetBrowserContext());
}

GPMEnclaveController::AccountState
GPMEnclaveController::account_state_for_testing() const {
  return account_state_;
}

void GPMEnclaveController::OnEnclaveLoaded() {
  CHECK_EQ(account_state_, AccountState::kLoading);

  if (enclave_manager_->is_ready()) {
    FIDO_LOG(EVENT) << "Enclave is ready";
    SetAccountStateReady();
  } else {
    FIDO_LOG(EVENT) << "Account state needs to be checked";
    account_state_ = AccountState::kChecking;
    DownloadAccountState(GetProfile());
  }

  model_->OnReadyForUI();
}

void GPMEnclaveController::DownloadAccountState(Profile* profile) {
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory();
  std::unique_ptr<trusted_vault::TrustedVaultConnection> trusted_vault_conn =
      vault_connection_override_
          ? std::move(vault_connection_override_)
          : trusted_vault::NewFrontendTrustedVaultConnection(
                trusted_vault::SecurityDomainId::kPasskeys, identity_manager,
                url_loader_factory);
  auto* conn = trusted_vault_conn.get();
  download_account_state_request_ =
      conn->DownloadAuthenticationFactorsRegistrationState(
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin),
          base::BindOnce(&GPMEnclaveController::OnAccountStateDownloaded,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(trusted_vault_conn)));
}

void GPMEnclaveController::OnAccountStateDownloaded(
    std::unique_ptr<trusted_vault::TrustedVaultConnection> unused,
    trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
        result) {
  using Result =
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;
  download_account_state_request_.reset();

  const char* state_str;
  switch (result.state) {
    case Result::State::kError:
      state_str = "Error";
      account_state_ = AccountState::kNone;
      break;

    case Result::State::kEmpty:
      state_str = "Empty";
      account_state_ = AccountState::kEmpty;
      break;

    case Result::State::kRecoverable:
      state_str = "Recoverable";
      account_state_ = AccountState::kRecoverable;
      break;

    case Result::State::kIrrecoverable:
      state_str = "Irrecoverable";
      account_state_ = AccountState::kIrrecoverable;
      break;
  }

  FIDO_LOG(EVENT) << "Download account state result: " << state_str
                  << ", key_version: " << result.key_version.value_or(0)
                  << ", has PIN: " << result.gpm_pin_metadata.has_value();

  if (result.gpm_pin_metadata) {
    pin_metadata_ = std::move(result.gpm_pin_metadata);
  }

  if (waiting_for_account_state_to_start_enclave_) {
    waiting_for_account_state_to_start_enclave_ = false;
    OnGPMSelected();
  }
}

void GPMEnclaveController::OnKeysStored() {
  if (model_->step() != Step::kRecoverSecurityDomain) {
    return;
  }
  CHECK(enclave_manager_->has_pending_keys());
  CHECK(!enclave_manager_->is_ready());

  if (pin_metadata_.has_value()) {
    // The account already has a GPM PIN.
    if (!enclave_manager_->AddDeviceToAccount(
            std::move(pin_metadata_),
            base::BindOnce(&GPMEnclaveController::OnDeviceAdded,
                           weak_ptr_factory_.GetWeakPtr()))) {
      model_->SetStep(Step::kGPMError);
    }
  } else {
    // If the user has local biometrics, and an existing recovery factor,
    // we'll likely choose not to create a GPM PIN. For now, however, we
    // always do:
    model_->SetStep(Step::kGPMCreatePin);
  }
}

void GPMEnclaveController::OnDeviceAdded(bool success) {
  if (!success) {
    model_->SetStep(Step::kGPMError);
    return;
  }

  have_added_device_ = true;
  SetAccountStateReady();

  switch (account_state_) {
    case AccountState::kReady:
      StartTransaction();
      break;

    case AccountState::kReadyWithBiometrics:
      model_->SetStep(Step::kGPMTouchID);
      break;

    default:
      // kReadyWithPIN is not possible because `have_added_device_` is set
      // and so user verification will be satisfied with the stored security
      // domain secret in this case.
      NOTREACHED_NORETURN();
  }
}

void GPMEnclaveController::SetAccountStateReady() {
  switch (PickEnclaveUserVerificationMethod(
      user_verification_requirement_, have_added_device_,
      enclave_manager_->has_wrapped_pin(), enclave_manager_->uv_key_state())) {
    case EnclaveUserVerificationMethod::kUVKeyWithSystemUI:
    case EnclaveUserVerificationMethod::kNone:
    case EnclaveUserVerificationMethod::kImplicit:
      account_state_ = AccountState::kReady;
      break;

    case EnclaveUserVerificationMethod::kPIN:
      account_state_ = AccountState::kReadyWithPIN;
      break;

    case EnclaveUserVerificationMethod::kUVKeyWithChromeUI:
      account_state_ = AccountState::kReadyWithBiometrics;
      break;

    case EnclaveUserVerificationMethod::kUnsatisfiable:
      account_state_ = AccountState::kNone;
      break;
  }

  pin_is_arbitrary_ = enclave_manager_->has_wrapped_pin() &&
                      enclave_manager_->wrapped_pin_is_arbitrary();
}

void GPMEnclaveController::OnGPMSelected() {
  switch (account_state_) {
    case AccountState::kReady:
    case AccountState::kReadyWithPIN:
      model_->SetStep(Step::kGPMCreatePasskey);
      break;

    case AccountState::kReadyWithBiometrics:
      model_->SetStep(Step::kGPMTouchID);
      break;

    case AccountState::kRecoverable:
      model_->SetStep(Step::kTrustThisComputerCreation);
      break;

    case AccountState::kLoading:
    case AccountState::kChecking:
      waiting_for_account_state_to_start_enclave_ = true;
      model_->ui_disabled_ = true;
      model_->OnSheetModelChanged();
      break;

    case AccountState::kNone:
      NOTREACHED();
      break;

    case AccountState::kIrrecoverable:
      // TODO(enclave): show the reset flow.
      NOTIMPLEMENTED();
      break;

    case AccountState::kEmpty:
      model_->SetStep(Step::kGPMOnboarding);
      break;
  }
}

void GPMEnclaveController::OnGPMPasskeySelected(
    base::span<const uint8_t> credential_id) {
  selected_cred_id_ = std::vector(credential_id.begin(), credential_id.end());

  switch (account_state_) {
    case AccountState::kReady:
      StartTransaction();
      break;

    case AccountState::kReadyWithPIN:
      PromptForPin();
      break;

    case AccountState::kReadyWithBiometrics:
      model_->SetStep(Step::kGPMTouchID);
      break;

    case AccountState::kRecoverable:
      if (model_->priority_phone_name.has_value()) {
        model_->SetStep(Step::kTrustThisComputerAssertion);
      } else {
        model_->SetStep(Step::kRecoverSecurityDomain);
      }
      break;

    case AccountState::kLoading:
    case AccountState::kChecking:
      // TODO(enclave): need to disable the UI elements.
      NOTIMPLEMENTED();
      break;

    case AccountState::kNone:
    case AccountState::kIrrecoverable:
      if (model_->priority_phone_name.has_value()) {
        model_->ContactPriorityPhone();
      } else {
        NOTIMPLEMENTED();
      }
      break;

    case AccountState::kEmpty:
      if (model_->priority_phone_name.has_value()) {
        model_->ContactPriorityPhone();
      } else {
        // TODO(enclave): the security domain is empty but there were
        // sync entities. Most like the security domain was reset without
        // clearing the entities, thus they are unusable. We have not yet
        // decided what the behaviour will be in this case.
        NOTIMPLEMENTED();
      }
      break;
  }
}

void GPMEnclaveController::PromptForPin() {
  model_->SetStep(pin_is_arbitrary_ ? Step::kGPMEnterArbitraryPin
                                    : Step::kGPMEnterPin);
}

void GPMEnclaveController::OnTrustThisComputer() {
  CHECK(model_->step() == Step::kTrustThisComputerAssertion ||
        model_->step() == Step::kTrustThisComputerCreation);
  model_->SetStep(Step::kRecoverSecurityDomain);
}

void GPMEnclaveController::OnGPMOnboardingAccepted() {
  DCHECK_EQ(model_->step(), Step::kGPMOnboarding);
  model_->SetStep(Step::kGPMCreatePin);
}

void GPMEnclaveController::OnGPMPinOptionChanged(bool is_arbitrary) {
  CHECK(model_->step() == Step::kGPMCreatePin ||
        model_->step() == Step::kGPMCreateArbitraryPin);
  model_->SetStep(is_arbitrary ? Step::kGPMCreateArbitraryPin
                               : Step::kGPMCreatePin);
}

void GPMEnclaveController::OnGPMCreatePasskey() {
  DCHECK_EQ(model_->step(), Step::kGPMCreatePasskey);
  DCHECK(account_state_ == AccountState::kReady ||
         account_state_ == AccountState::kReadyWithPIN ||
         account_state_ == AccountState::kReadyWithBiometrics);
  if (account_state_ == AccountState::kReady) {
    StartTransaction();
  } else if (account_state_ == AccountState::kReadyWithPIN) {
    PromptForPin();
  } else if (account_state_ == AccountState::kReadyWithBiometrics) {
    model_->SetStep(Step::kGPMTouchID);
  } else {
    NOTREACHED_NORETURN();
  }
}

void GPMEnclaveController::OnGPMPinEntered(const std::u16string& pin) {
  DCHECK(model_->step() == Step::kGPMCreateArbitraryPin ||
         model_->step() == Step::kGPMCreatePin ||
         model_->step() == Step::kGPMEnterArbitraryPin ||
         model_->step() == Step::kGPMEnterPin);
  pin_ = base::UTF16ToUTF8(pin);

  // TODO(enclave): jump to spinner state here? The PIN entry will still
  // be showing so should, at least, be disabled.

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
  } else {
    StartTransaction();
  }
}

void GPMEnclaveController::OnTouchIDComplete(bool success) {
  // On error no LAContext will be provided and macOS will show the system UI
  // for user verification.
  StartTransaction();
}

void GPMEnclaveController::StartTransaction() {
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

  if (!token) {
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

  switch (PickEnclaveUserVerificationMethod(
      user_verification_requirement_, have_added_device_,
      enclave_manager_->has_wrapped_pin(), enclave_manager_->uv_key_state())) {
    case EnclaveUserVerificationMethod::kNone:
      request->signing_callback =
          enclave_manager_->HardwareKeySigningCallback();
      break;

    case EnclaveUserVerificationMethod::kImplicit:
      request->signing_callback =
          enclave_manager_->HardwareKeySigningCallback();
      use_unwrapped_secret = true;
      break;

    case EnclaveUserVerificationMethod::kPIN:
      request->signing_callback =
          enclave_manager_->HardwareKeySigningCallback();
      CHECK(claimed_pin);
      request->claimed_pin = std::move(claimed_pin);
      break;

    case EnclaveUserVerificationMethod::kUVKeyWithChromeUI:
    case EnclaveUserVerificationMethod::kUVKeyWithSystemUI: {
      EnclaveManager::UVKeyOptions uv_options;
#if BUILDFLAG(IS_MAC)
      uv_options.lacontext = std::move(model_->lacontext);
#endif  // BUILDFLAG(IS_MAC)
      request->signing_callback =
          enclave_manager_->UserVerifyingKeySigningCallback(
              std::move(uv_options));
      break;
    }
    case EnclaveUserVerificationMethod::kUnsatisfiable:
      NOTREACHED_NORETURN();
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
}
