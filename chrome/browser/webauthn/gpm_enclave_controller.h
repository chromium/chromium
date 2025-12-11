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
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/gpm_enclave_transaction.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/global_routing_id.h"
#include "google_apis/gaia/gaia_id.h"

namespace base {
class TickClock;
class SequencedTaskRunner;
}  // namespace base

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace device {
class FidoDiscoveryFactory;
enum class FidoRequestType : uint8_t;
enum class UserVerificationRequirement;
namespace enclave {
struct CredentialRequest;
}  // namespace enclave
}  // namespace device

namespace sync_pb {
class WebauthnCredentialSpecifics;
}  // namespace sync_pb

namespace trusted_vault {
class ICloudRecoveryKey;
}  // namespace trusted_vault

enum class EnclaveEnabledStatus;
class Profile;

// Provides a TrustedVaultConnection for a given RenderFrameHost.
// This allows tests to override the connection used by GPMEnclaveController.
class GpmTrustedVaultConnectionProvider
    : public content::DocumentUserData<GpmTrustedVaultConnectionProvider> {
 public:
  ~GpmTrustedVaultConnectionProvider() override;

  // Sets a TrustedVaultConnection override for the document associated with
  // `rfh`. The next call to GetConnectionForFrame for this document will
  // return this override.
  static void SetOverrideForFrame(
      content::RenderFrameHost* rfh,
      std::unique_ptr<trusted_vault::TrustedVaultConnection>
          connection_override);

  // Returns a TrustedVaultConnection for the document associated with `rfh`.
  // If an override has been set via SetOverrideForFrame, that override is
  // returned (and ownership is transferred). Otherwise, a new default
  // TrustedVaultConnection is created. That connection is not associated with
  // any particular document.
  static std::unique_ptr<trusted_vault::TrustedVaultConnection> GetConnection(
      content::RenderFrameHost* rfh,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  explicit GpmTrustedVaultConnectionProvider(content::RenderFrameHost* rfh);

  friend class content::DocumentUserData<GpmTrustedVaultConnectionProvider>;
  DOCUMENT_USER_DATA_KEY_DECL();

  std::unique_ptr<trusted_vault::TrustedVaultConnection> connection_override_;
};

class GpmTickAndTaskRunnerProvider
    : public content::DocumentUserData<GpmTickAndTaskRunnerProvider> {
 public:
  ~GpmTickAndTaskRunnerProvider() override;

  // Sets a TickClock and SequencedTaskRunner override for the document
  // associated with |rfh|. The next call to GetConnectionForFrame for this
  // document will return this override.
  static void SetOverrideForFrame(
      content::RenderFrameHost* rfh,
      base::TickClock const* tick_clock,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Returns the TickClock for the document associated with `rfh` if an override
  // has been set via SetOverrideForFrame. Otherwise, the default TickClock is
  // returned.
  static base::TickClock const* GetTickClock(content::RenderFrameHost* rfh);

  // Returns the SequencedTaskRunner for the document associated with `rfh` if
  // an override has been set via SetOverrideForFrame. Otherwise, nullptr` is
  // returned.
  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunner(
      content::RenderFrameHost* rfh);

 private:
  explicit GpmTickAndTaskRunnerProvider(content::RenderFrameHost* rfh);
  friend class content::DocumentUserData<GpmTickAndTaskRunnerProvider>;
  DOCUMENT_USER_DATA_KEY_DECL();

  raw_ptr<base::TickClock const> tick_clock_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

class GPMEnclaveController : public AuthenticatorRequestDialogModel::Observer,
                             public EnclaveManager::Observer,
                             public GPMEnclaveTransaction::Delegate {
 public:
  static constexpr base::TimeDelta kLoadingTimeout = base::Milliseconds(500);

  enum class AccountState {
    // There isn't a primary account, or enclave support is disabled.
    kNone,
    // The GPM state is still being loaded. This may be loading the enclave
    // state from disk, checking for biometric availability, or pending network
    // requests.
    kLoading,
    // The account can be recovered via user action.
    kRecoverable,
    // The account cannot be recovered, but could be reset.
    kIrrecoverable,
    // The security domain is empty.
    kEmpty,
    // The enclave is ready to use.
    kReady,
  };

  enum class AccountReadyState {
    kNotReady,
    kLoading,
    kReady,
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

  // Determines the enclave user verification early depending on the enclave
  // state and UV requirements. Can return `std::nullopt` if the enclave is not
  // ready. This is used for immediate mode requests.
  std::optional<EnclaveUserVerificationMethod>
  GetEnclaveUserVerificationMethod();

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

  // Returns the ready state of the account.
  AccountReadyState account_ready_state() const;
  // Runs `callback` once the account state is no longer `kLoading` or
  // `kChecking`. If it's already in such a state, runs it immediately.
  void RunWhenAccountReady(base::OnceClosure callback);

  base::RepeatingCallback<
      void(std::unique_ptr<device::enclave::CredentialRequest>)>&
  enclave_request_callback_for_testing() {
    return enclave_request_callback_;
  }

 private:
  // GPMEnclaveTransaction::Delegate:
  void HandleEnclaveTransactionError() override;
  void BuildUVKeyOptions(EnclaveManager::UVKeyOptions& options) override;
  void HandlePINValidationResult(
      device::enclave::PINValidationResult result) override;
  void OnPasskeyCreated(
      const sync_pb::WebauthnCredentialSpecifics& passkey) override;
  EnclaveUserVerificationMethod GetUvMethod() override;

  Profile* GetProfile() const;

  void OnUVCapabilityKnown(bool can_create_uv_keys);

  // Called when the EnclaveManager has finished loading its state from the
  // disk.
  void OnEnclaveLoaded();

  // Starts downloading the state of the account from the security domain
  // service.
  void DownloadAccountState();

  // Called when the account state has finished downloading.
  void OnAccountStateDownloaded(
      GaiaId gaia_id,
      std::unique_ptr<trusted_vault::TrustedVaultConnection> unused,
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
          result);

  // Called when enough state has been loaded that the initial UI can be shown.
  // If `kEnabled` then the enclave will be a valid mechanism.
  void SetActive(EnclaveEnabledStatus enclave_enabled_status);

  // EnclaveManager::Observer:
  void OnKeysStored() override;
  void OnOutOfContextRecoveryCompletion(
      EnclaveManager::OutOfContextRecoveryOutcome outcome) override;

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
      std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>
          local_icloud_keys);

  // Enrolls a specific iCloud keychain recovery key. |key| may be null, in
  // which case we skip to the next step.
  void EnrollICloudRecoveryKey(
      std::unique_ptr<trusted_vault::ICloudRecoveryKey> key);

  // Called when Chrome has retrieved the iCloud recovery keys present in the
  // current device.
  void OnICloudKeysRetrievedForRecovery(
      std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>
          local_icloud_keys);
#endif  // BUILDFLAG(IS_MAC)

  // Called when the enclave enrollment is complete.
  void OnEnclaveAccountSetUpComplete();

  // Called when the EnclaveManager has finished loading. Sets `account_state_`
  // and progresses the flow if waiting.
  void SetAccountState(AccountState account_state);

  // Sets the UI to the correct PIN prompt for the type of PIN configured.
  void PromptForPin();

  // Called when the user completes forgot pin flow.
  void OnGpmPinChanged(bool success);

  // Called when the user selects a GPM option, but the enclave is still loading
  // or the account data hasn't finished downloading yet.
  void OnGpmSelectedWhileLoading();

  // Called when the enclave is still loading and |loading_timeout_| is
  // triggered.
  void OnLoadingTimeout();

  // AuthenticatorRequestDialogModel::Observer:
  void OnGPMSelected() override;
  void OnGPMPasskeySelected(std::vector<uint8_t> credential_id) override;
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

  // Accessors for the profile pref that counts the number of consecutive failed
  // PIN attempts to know when a lockout will happen.
  int GetFailedPINAttemptCount();
  void SetFailedPINAttemptCount(int count);

  // BrowserIsApp returns true if the current `Browser` is `TYPE_APP`. (I.e. a
  // PWA.)
  bool BrowserIsApp() const;

  // Configures the user-visible method of authenticating for security domain
  // recovery.
  void ShowSecurityDomainRecoveryUI();

  void RefreshStateAndRepeatOperation();
  bool ShouldRefreshState();

  content::WebContents* web_contents() const;

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

  AccountState account_state_ = AccountState::kLoading;
  bool pin_is_arbitrary_ = false;
  std::optional<std::string> pin_;
  std::vector<sync_pb::WebauthnCredentialSpecifics> creds_;

  // The user verification that will be performed for this request.
  std::optional<EnclaveUserVerificationMethod> uv_method_;

  std::optional<bool> is_active_;

  // Whether the system can make UV keys.
  std::optional<bool> can_make_uv_keys_;

  // have_added_device_ is set to true if the local device was added to the
  // security domain during this transaction. In this case, the security domain
  // secret is available and can be used to satisfy user verification.
  bool have_added_device_ = false;

  // The ID of the selected credential when doing a get().
  std::optional<std::vector<uint8_t>> selected_cred_id_;

  // Contains the bytes of a WrappedPIN structure, downloaded from the security
  // domain service. This is only set if the PIN is usable for recovery.
  std::optional<trusted_vault::GpmPinMetadata> pin_metadata_;

  // The list of iCloud recovery key members known to the security domain
  // service.
  std::vector<trusted_vault::VaultMember> security_domain_icloud_recovery_keys_;

  // |recovered_with_icloud_keychain_| is true if this controller performed a
  // successful recovery from iCloud keychain. This is reset on OnKeysStored().
  bool recovered_with_icloud_keychain_ = false;

  // The pending request to fetch the state of the trusted vault.
  std::unique_ptr<trusted_vault::TrustedVaultConnection::Request>
      download_account_state_request_;

  std::unique_ptr<GPMEnclaveTransaction> pending_enclave_transaction_;

  // The callback used to trigger a request to the enclave.
  base::RepeatingCallback<void(
      std::unique_ptr<device::enclave::CredentialRequest>)>
      enclave_request_callback_;

  // Represents this object's claim to handle any keys provided by
  // accounts.google.com.
  std::unique_ptr<EnclaveManager::StoreKeysLock> store_keys_lock_;

  // Whether the initial UI is being blocked while enclave state is loaded.
  bool ready_for_ui_ = false;

  // Whether showing the UI was delayed because the result from the security
  // domain service is needed.
  base::OnceClosure waiting_for_account_state_;

  // If changing a GPM PIN, this holds a ReAuthentication Proof Token (RAPT), if
  // the user is authenticating the request via doing a GAIA reauth.
  std::optional<std::string> rapt_ = std::nullopt;

  // A timeout to prevent waiting for the enclave to load forever. If triggered
  // while still loading, the user is sent to the mechanism selection screen.
  // Loading the enclave and downloading account data are not interrupted.
  base::OneShotTimer loading_timeout_;

  // Set to true when the user initiates reset GPM pin flow during UV.
  bool changing_gpm_pin_ = false;

  // Records when the user has confirmed credential creation in an Incognito
  // context.
  bool off_the_record_confirmed_ = false;

  // Whether the user confirmed GPM PIN creation in the flow.
  bool gpm_pin_creation_confirmed_ = false;

  bool is_state_stale_ = false;

  // The gaia id of the user at the time the account state was downloaded.
  GaiaId user_gaia_id_;

  base::WeakPtrFactory<GPMEnclaveController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_ENCLAVE_CONTROLLER_H_
