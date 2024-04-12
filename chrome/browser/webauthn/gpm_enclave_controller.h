// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_ENCLAVE_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_ENCLAVE_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace device {
class FidoDiscoveryFactory;
enum class FidoRequestType : uint8_t;
enum class UserVerificationRequirement;
namespace enclave {
struct CredentialRequest;
}  // namespace enclave
}  // namespace device

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

class Profile;

class GPMEnclaveController : AuthenticatorRequestDialogModel::Observer,
                             EnclaveManager::Observer {
 public:
  enum class AccountState {
    // There isn't a primary account, or enclave support is disabled.
    kNone,
    // The enclave state is still being loaded from disk.
    kLoading,
    // The state of the account is unknown pending network requests.
    kChecking,
    // The account can be recovered via user action.
    kRecoverable,
    // The account cannot be recovered, but could be reset.
    kIrrecoverable,
    // The security domain is empty.
    kEmpty,
    // The enclave is ready to use.
    kReady,
    // The enclave is ready to use, but the UI needs to collect a PIN before
    // making a transaction.
    kReadyWithPIN,
    // The enclave is ready to use, but the UI needs to collect biometrics
    // before making a transaction.
    kReadyWithBiometrics,
  };

  explicit GPMEnclaveController(
      content::RenderFrameHost* render_frame_host,
      AuthenticatorRequestDialogModel* model,
      const std::string& rp_id,
      device::FidoRequestType request_type,
      device::UserVerificationRequirement user_verification_requirement);
  GPMEnclaveController(const GPMEnclaveController&) = delete;
  GPMEnclaveController& operator=(const GPMEnclaveController&) = delete;
  GPMEnclaveController(GPMEnclaveController&&) = delete;
  GPMEnclaveController& operator=(GPMEnclaveController&&) = delete;
  ~GPMEnclaveController() override;

  // Returns true if the enclave state is loaded to the point where the UI
  // can be shown. If false, then the `OnReadyForUI` event will be triggered
  // on the model when ready.
  bool ready_for_ui() const;

  // Configures an WebAuthn enclave authenticator discovery and provides it with
  // synced passkeys.
  void ConfigureDiscoveries(device::FidoDiscoveryFactory* discovery_factory);

  // Fetch the set of GPM passkeys for this request.
  const std::vector<sync_pb::WebauthnCredentialSpecifics>& creds() const;

  // Allows setting a mock `TrustedVaultConnection` so a real one will not be
  // created. This is only used for a single request, and is destroyed
  // afterwards.
  void SetTrustedVaultConnectionForTesting(
      std::unique_ptr<trusted_vault::TrustedVaultConnection> connection);

  AccountState account_state_for_testing() const;

 private:
  Profile* GetProfile() const;

  // Called when the EnclaveManager has finished loading its state from the
  // disk.
  void OnEnclaveLoaded();

  // Starts downloading the state of the account from the security domain
  // service.
  void DownloadAccountState(Profile* profile);

  // Called when the account state has finished downloading.
  void OnAccountStateDownloaded(
      std::unique_ptr<trusted_vault::TrustedVaultConnection> unused,
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
          result);

  // EnclaveManager::Observer:
  void OnKeysStored() override;

  // Called when the local device has been added to the security domain.
  void OnDeviceAdded(bool success);

  // Called when the EnclaveManager is ready. Sets `account_state_` to the
  // correct value for the level of user verification required.
  void SetAccountStateReady();

  // Called when the user selects Google Password Manager from the list of
  // mechanisms. (Or when it's the priority mechanism.)
  void OnGPMSelected() override;

  // Called when a GPM passkey is selected from a list of credentials.
  void OnGPMPasskeySelected(base::span<const uint8_t> credential_id) override;

  // Sets the UI to the correct PIN prompt for the type of PIN configured.
  void PromptForPin();

  // AuthenticatorRequestDialogModel::Observer:
  void OnTrustThisComputer() override;
  void OnGPMOnboardingAccepted() override;
  void OnGPMPinOptionChanged(bool is_arbitrary) override;
  void OnGPMCreatePasskey() override;
  void OnGPMPinEntered(const std::u16string& pin) override;
  void OnTouchIDComplete(bool success) override;

  // Starts a create() or get() action with the enclave.
  void StartTransaction();

  // Called when the UI has reached a state where it needs to do an enclave
  // operation, and an OAuth token for the enclave has been fetched.
  void MaybeHashPinAndStartEnclaveTransaction(std::optional<std::string> token);

  // Called when the UI has reached a state where it needs to do an enclave
  // operation, an OAuth token for the enclave has been fetched, and any PIN
  // hashing has been completed.
  void StartEnclaveTransaction(std::optional<std::string> token,
                               std::unique_ptr<device::enclave::ClaimedPIN>);

  // Invoked when a new GPM passkey is created, to save it to sync data.
  void OnPasskeyCreated(sync_pb::WebauthnCredentialSpecifics passkey);

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  const std::string rp_id_;
  const device::FidoRequestType request_type_;
  const device::UserVerificationRequirement user_verification_requirement_;

  // The `EnclaveManager` is a `KeyedService` for the current profile and so
  // outlives this object.
  const raw_ptr<EnclaveManager> enclave_manager_;

  // This is owned by the ChromeAuthenticatorRequestDelegate, which also owns
  // this object.
  const raw_ptr<AuthenticatorRequestDialogModel> model_;

  base::ScopedObservation<
      base::ObserverList<AuthenticatorRequestDialogModel::Observer>,
      AuthenticatorRequestDialogModel::Observer>
      model_observer_{this};
  base::ScopedObservation<EnclaveManager, EnclaveManager::Observer>
      enclave_manager_observer_{this};

  AccountState account_state_ = AccountState::kNone;
  bool pin_is_arbitrary_ = false;
  std::optional<std::string> pin_;
  std::vector<sync_pb::WebauthnCredentialSpecifics> creds_;

  // have_added_device_ is set to true if the local device was added to the
  // security domain during this transaction. In this case, the security domain
  // secret is available and can be used to satisfy user verification.
  bool have_added_device_ = false;

  // The ID of the selected credential when doing a get().
  std::optional<std::vector<uint8_t>> selected_cred_id_;

  // Contains the bytes of a WrappedPIN structure, downloaded from the security
  // domain service.
  std::optional<trusted_vault::GpmPinMetadata> pin_metadata_;

  // The pending request to fetch the state of the trusted vault.
  std::unique_ptr<trusted_vault::TrustedVaultConnection::Request>
      download_account_state_request_;

  // The pending request to fetch an OAuth token for the enclave request.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // The callback used to trigger a request to the enclave.
  base::RepeatingCallback<void(
      std::unique_ptr<device::enclave::CredentialRequest>)>
      enclave_request_callback_;

  // Override for test mocking.
  std::unique_ptr<trusted_vault::TrustedVaultConnection>
      vault_connection_override_;

  // Whether showing the UI was delayed because the result from the security
  // domain service is needed.
  bool waiting_for_account_state_to_start_enclave_ = false;

  base::WeakPtrFactory<GPMEnclaveController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_ENCLAVE_CONTROLLER_H_
