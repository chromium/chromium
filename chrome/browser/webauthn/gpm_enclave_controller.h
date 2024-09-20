// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_ENCLAVE_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_ENCLAVE_CONTROLLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "content/public/browser/global_routing_id.h"

namespace base {
class Clock;
}

namespace content {
class RenderFrameHost;
}  // namespace content

namespace device {
class FidoDiscoveryFactory;
enum class FidoRequestType : uint8_t;
enum class UserVerificationRequirement;
namespace enclave {
struct CredentialRequest;
class ICloudRecoveryKey;
}  // namespace enclave
}  // namespace device

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace sync_pb {
class WebauthnCredentialSpecifics;
}  // namespace sync_pb

class Profile;

class GPMEnclaveController : AuthenticatorRequestDialogModel::Observer,
                             EnclaveManager::Observer {
 public:
  static constexpr base::TimeDelta kDownloadAccountStateTimeout =
      base::Seconds(1);
  struct ICloudMember;
  struct DownloadedAccountState;
  enum class EnclaveUserVerificationMethod;

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
  };

  explicit GPMEnclaveController(
      content::RenderFrameHost* render_frame_host,
      AuthenticatorRequestDialogModel* model,
      const std::string& rp_id,
      device::FidoRequestType request_type,
      device::UserVerificationRequirement user_verification_requirement,
      base::Clock* clock,
      // `optional_connection` can be set to override the connection to the
      // security domain service for testing.
      std::unique_ptr<trusted_vault::TrustedVaultConnection>
          optional_connection);
  GPMEnclaveController(const GPMEnclaveController&) = delete;
  GPMEnclaveController& operator=(const GPMEnclaveController&) = delete;
  GPMEnclaveController(GPMEnclaveController&&) = delete;
  GPMEnclaveController& operator=(GPMEnclaveController&&) = delete;
  ~GPMEnclaveController() override;

  // Returns true if the enclave is active for this request. Crashes the address
  // space if this hasn't yet been resolved.
  bool is_active() const;

  // Returns true if the enclave state is loaded to the point where the UI
  // can be shown. If false, then the `OnReadyForUI` event will be triggered
  // on the model when ready.
  bool ready_for_ui() const;

  // Configures an WebAuthn enclave authenticator discovery and provides it with
  // synced passkeys.
  void ConfigureDiscoveries(device::FidoDiscoveryFactory* discovery_factory);

  // Fetch the set of GPM passkeys for this request.
  const std::vector<sync_pb::WebauthnCredentialSpecifics>& creds() const;

  AccountState account_state_for_testing() const;

 private:
  Profile* GetProfile() const;

  void OnUVCapabilityKnown(bool can_create_uv_keys);

  // Called when the EnclaveManager has finished loading its state from the
  // disk.
  void OnEnclaveLoaded();

  // Starts downloading the state of the account from the security domain
  // service.
  void DownloadAccountState();

  // Called when fetching the account state took too long.
  void OnAccountStateTimeOut();

  // Called when the account state has finished downloading.
  void OnAccountStateDownloaded(
      std::string gaia_id,
      std::unique_ptr<trusted_vault::TrustedVaultConnection> unused,
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
          result);

  void OnHaveAccountState(DownloadedAccountState result);

  // Called when enough state has been loaded that the initial UI can be shown.
  // If `active` then the enclave will be a valid mechanism.
  void SetActive(bool active);

  // EnclaveManager::Observer:
  void OnKeysStored() override;

  // Called when the local device has been added to the security domain.
  void OnDeviceAdded(bool success);

  // Initiates recovery from an iCloud keychain recovery key or MagicArch
  // depending on availability.
  void RecoverSecurityDomain();

#if BUILDFLAG(IS_MAC)
  // Enrolls an iCloud keychain recovery factor if available and needed.
  void MaybeAddICloudRecoveryKey();

  // Called when Chrome has retrieved the iCloud recovery keys present in the
  // current device.
  void OnICloudKeysRetrievedForEnrollment(
      std::vector<std::unique_ptr<device::enclave::ICloudRecoveryKey>>
          local_icloud_keys);

  // Enrolls a specific iCloud keychain recovery key. |key| may be null, in
  // which case we skip to the next step.
  void EnrollICloudRecoveryKey(
      std::unique_ptr<device::enclave::ICloudRecoveryKey> key);

  // Called when Chrome has retrieved the iCloud recovery keys present in the
  // current device.
  void OnICloudKeysRetrievedForRecovery(
      std::vector<std::unique_ptr<device::enclave::ICloudRecoveryKey>>
          local_icloud_keys);
#endif  // BUILDFLAG(IS_MAC)

  // Called when the enclave enrollment is complete.
  void OnEnclaveAccountSetUpComplete();

  // Called when the EnclaveManager is ready. Sets `account_state_` to the
  // correct value for the level of user verification required.
  void SetAccountStateReady();

  // Called when the user selects Google Password Manager from the list of
  // mechanisms. (Or when it's the priority mechanism.)
  void OnGPMSelected() override;

  // Called when a GPM passkey is selected from a list of credentials.
  void OnGPMPasskeySelected(std::vector<uint8_t> credential_id) override;

  // Sets the UI to the correct PIN prompt for the type of PIN configured.
  void PromptForPin();

  // Called when the user completes forgot pin flow.
  void OnGpmPinChanged(bool success);

  // AuthenticatorRequestDialogModel::Observer:
  void OnTrustThisComputer() override;
  void OnGPMPinOptionChanged(bool is_arbitrary) override;
  void OnGPMCreatePasskey() override;
  void OnGPMConfirmOffTheRecordCreate() override;
  void OnGPMPinEntered(const std::u16string& pin) override;
  void OnTouchIDComplete(bool success) override;
  void OnForgotGPMPinPressed() override;
  void OnReauthComplete(std::string rapt) override;
  void OnGpmPasskeysReset(bool success) override;

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

  // Accessors for the profile pref that counts the number of consecutive failed
  // PIN attempts to know when a lockout will happen.
  int GetFailedPINAttemptCount();
  void SetFailedPINAttemptCount(int count);

  // Invoked when a passkey request has been sent to the enclave service with
  // PIN UV, and the request succeeded or a PIN validation error occurred.
  void HandlePINValidationResult(device::enclave::PINValidationResult type);

  // BrowserIsApp returns true if the current `Browser` is `TYPE_APP`. (I.e. a
  // PWA.)
  bool BrowserIsApp() const;

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

  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      model_observer_{this};
  base::ScopedObservation<EnclaveManager, EnclaveManager::Observer>
      enclave_manager_observer_{this};

  AccountState account_state_ = AccountState::kNone;
  bool pin_is_arbitrary_ = false;
  std::optional<std::string> pin_;
  std::vector<sync_pb::WebauthnCredentialSpecifics> creds_;

  // The user verification that will be performed for this request.
  std::optional<EnclaveUserVerificationMethod> uv_method_;

  std::optional<bool> is_active_ = false;

  // Whether the system can make UV keys.
  std::optional<bool> can_make_uv_keys_;

  // have_added_device_ is set to true if the local device was added to the
  // security domain during this transaction. In this case, the security domain
  // secret is available and can be used to satisfy user verification.
  bool have_added_device_ = false;

  // The ID of the selected credential when doing a get().
  std::optional<std::vector<uint8_t>> selected_cred_id_;

  // Contains the bytes of a WrappedPIN structure, downloaded from the security
  // domain service.
  std::optional<trusted_vault::GpmPinMetadata> pin_metadata_;

  // The list of iCloud recovery key members known to the security domain
  // service.
  std::vector<ICloudMember> security_domain_icloud_recovery_keys_;

  // |recovered_with_icloud_keychain_| is true if this controller performed a
  // successful recovery from iCloud keychain. This is reset on OnKeysStored().
  bool recovered_with_icloud_keychain_ = false;

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

  // Whether the initial UI is being blocked while enclave state is loaded.
  bool ready_for_ui_ = false;

  // Whether showing the UI was delayed because the result from the security
  // domain service is needed.
  base::OnceClosure waiting_for_account_state_;

  // If changing a GPM PIN, this holds a ReAuthentication Proof Token (RAPT), if
  // the user is authenticating the request via doing a GAIA reauth.
  std::optional<std::string> rapt_ = std::nullopt;

  // A timeout to prevent waiting for the security domain service forever.
  std::unique_ptr<base::OneShotTimer> account_state_timeout_;

  // Set to true when the user initiates reset GPM pin flow during UV.
  bool changing_gpm_pin_ = false;

  // Records when the user has confirmed credential creation in an Incognito
  // context.
  bool off_the_record_confirmed_ = false;

  // Whether the user confirmed GPM PIN creation in the flow.
  bool gpm_pin_creation_confirmed_ = false;

  // The gaia id of the user at the time the account state was downloaded.
  std::string user_gaia_id_;

  const raw_ptr<base::Clock> clock_;

  base::WeakPtrFactory<GPMEnclaveController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_ENCLAVE_CONTROLLER_H_
