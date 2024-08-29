// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_H_
#define CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_H_

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "chrome/browser/webauthn/unexportable_key_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "content/public/browser/global_routing_id.h"
#include "crypto/user_verifying_key.h"
#include "device/fido/enclave/types.h"
#include "device/fido/network_context_factory.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/common/chrome_version.h"
#include "crypto/scoped_lacontext.h"
#endif  // BUILDFLAG(IS_MAC)

namespace crypto {
class RefCountedUserVerifyingSigningKey;
}  // namespace crypto

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
class WebAuthNDialogController;
}
#endif

#if BUILDFLAG(IS_MAC)
namespace device::enclave {
class ICloudRecoveryKey;
}  // namespace device::enclave
#endif  // BUILDFLAG(IS_MAC)

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace unexportable_keys {
class RefCountedUnexportableSigningKey;
}

namespace webauthn_pb {
class EnclaveLocalState;
class EnclaveLocalState_User;
class EnclaveLocalState_WrappedPIN;
}  // namespace webauthn_pb

namespace trusted_vault {
struct GpmPinMetadata;
class RecoveryKeyStoreConnection;
class TrustedVaultAccessTokenFetcherFrontend;
}  // namespace trusted_vault

// EnclaveManager stores and manages the passkey enclave state. One instance
// exists per-profile, owned by `EnclaveManagerFactory`.
//
// The state exposed from this class is per-primary-account. This class watches
// the `IdentityManager` and, when the primary account changes, the result of
// functions like `is_registered` will suddenly change too. If an account is
// removed from the cookie jar (and it's not primary) then state for that
// account will be erased. Any pending operations will be canceled when the
// primary account changes and their callback will be run with `false`.
//
// When `is_ready` is true then this class can produce wrapped security domain
// secrets and signing callbacks to use to perform passkey operations with the
// enclave, which is the ultimate point of this class.
class EnclaveManager : public EnclaveManagerInterface {
 public:
#if BUILDFLAG(IS_MAC)
  static constexpr char kEnclaveKeysKeychainAccessGroup[] =
      MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING
                                 ".webauthn-uvk";
#endif  // BUILDFLAG(IS_MAC)
  struct StoreKeysArgs;
  class Observer : public base::CheckedObserver {
   public:
    // OnKeyStores is called when MagicArch provides keys to the EnclaveManager
    // by calling `StoreKeys`.
    virtual void OnKeysStored() = 0;
  };

  struct UVKeyOptions {
    UVKeyOptions();
    UVKeyOptions(const UVKeyOptions&) = delete;
    UVKeyOptions& operator=(const UVKeyOptions&) = delete;
    UVKeyOptions(UVKeyOptions&&);
    UVKeyOptions& operator=(UVKeyOptions&&);
    ~UVKeyOptions();

    // The RP for the request, to be included in the UV dialog.
    std::string rp_id;

    // The RenderFrameHost from which the request originates.
    content::GlobalRenderFrameHostId render_frame_host_id;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::variant<raw_ptr<ash::WebAuthNDialogController>,
                 raw_ptr<ash::ActiveSessionAuthController>>
        dialog_controller;
#endif

#if BUILDFLAG(IS_MAC)
    // An optional LAcontext to pass to apple keychain operations.
    std::optional<crypto::ScopedLAContext> lacontext;
#endif  // BUILDFLAG(IS_MAC)
  };

  EnclaveManager(
      const base::FilePath& base_dir,
      signin::IdentityManager* identity_manager,
      device::NetworkContextFactory network_context_factory,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~EnclaveManager() override;
  EnclaveManager(const EnclaveManager&) = delete;
  EnclaveManager(const EnclaveManager&&) = delete;

  // Returns `this`.
  EnclaveManager* GetEnclaveManager() override;

  // Returns true if there are no current operations pending.
  bool is_idle() const;
  // Returns true if the persistent state has been loaded from the disk. (Or
  // else the loading failed and an empty state is being used.)
  bool is_loaded() const;
  // Returns true if the current user has been registered with the enclave.
  bool is_registered() const override;
  // Returns true if `StoreKeys` has been called and thus `AddDeviceToAccount`
  // or `AddDeviceAndPINToAccount` can be called.
  bool has_pending_keys() const;
  // Returns true if the current user has joined the security domain and has one
  // or more wrapped security domain secrets available. (This implies
  // `is_registered`.)
  bool is_ready() const;
  // Returns the number of times that `StoreKeys` has been called.
  unsigned store_keys_count() const;

  // Load the persisted state from disk. Harmless to call if `is_loaded`.
  void Load(base::OnceClosure closure);
  // Register with the enclave if not already registered.
  void RegisterIfNeeded(Callback callback);
  // Set up an account with a newly-created PIN.
  void SetupWithPIN(std::string pin, Callback callback);
  // Adds the current device to the security domain. Only valid to call after
  // `StoreKeys` has been called and thus `has_pending_keys` returns true. If
  // `pin_metadata` has a value then it is taken to be the current GPM PIN.
  // If you want to add a new PIN to the account, see
  // `AddDeviceAndPINToAccount`.
  //
  // Returns false if `serialized_wrapped_pin` fails to parse and true
  // otherwise.
  bool AddDeviceToAccount(
      std::optional<trusted_vault::GpmPinMetadata> pin_metadata,
      Callback callback);
  // Adds the current device, and a GPM PIN, to the security domain. Only valid
  // to call after `StoreKeys` has been called and thus `has_pending_keys`
  // returns true.
  void AddDeviceAndPINToAccount(std::string pin, Callback callback);
  // Change the GPM PIN on the account. If a RAPT (Reauthentication Proof Token)
  // is given then it will be used, otherwise the UV key will be used, causing
  // system UI to appear to verify the user.
  void ChangePIN(std::string updated_pin, std::string rapt, Callback callback);
  // Renew the current PIN. Requires `has_wrapped_pin` to be true.
  void RenewPIN(Callback callback);
#if BUILDFLAG(IS_MAC)
  // Adds an iCloud recovery key to the security domain. This can only be called
  // immediately after enrollment while we still have the security domain secret
  // around.
  void AddICloudRecoveryKey(
      std::unique_ptr<device::enclave::ICloudRecoveryKey> icloud_recovery_key,
      Callback callback);
#endif  // BUILDFLAG(IS_MAC)
  // Send a request to the enclave to delete the registration for the current
  // user, erase local keys, and erase local state for the user. Safe to call in
  // any state and is a no-op if no registration exists.
  void Unenroll(Callback callback) override;
  // Process the current security domain state. Requires `is_registered()`. This
  // can update the locally-cached view of the current GPM PIN, or can make
  // `is_ready()` false if the security domain has been reset.
  //
  // Returns whether `is_ready()` will return true in the future. (Because
  // other operations may be running at the time, is_ready() may not update
  // immediately.)
  bool ConsiderSecurityDomainState(
      const trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult&
          state,
      Callback callback);

  // Get a callback to sign with the registered "hw" key. Only valid to call if
  // `is_ready`.
  device::enclave::SigningCallback IdentityKeySigningCallback();
  // Get a callback to sign with the registered "uv" key. Only valid to call if
  // `is_ready`.
  device::enclave::SigningCallback UserVerifyingKeySigningCallback(
      UVKeyOptions options);
  // Get a callback that creates a new "uv" key. This can only be called when
  // `is_ready` and the user's state has `deferred_uv_key_creation` = true.
  // The callback will create a new UV key and provides the public key to the
  // invoker.
  device::enclave::UVKeyCreationCallback UserVerifyingKeyCreationCallback();
  // Fetch a wrapped security domain secret for the given epoch. Only valid to
  // call if `is_ready`.
  std::optional<std::vector<uint8_t>> GetWrappedSecret(int32_t version);
  // Get the version and value of the current wrapped secret. Only valid to call
  // if `is_ready`.
  std::pair<int32_t, std::vector<uint8_t>> GetCurrentWrappedSecret();
  // Take the security domain secret. Only possible immediately after the device
  // has been added to the account.
  std::optional<std::pair<int32_t, std::vector<uint8_t>>> TakeSecret();
  // Returns true if a wrapped PIN is available for the current user. Requires
  // `is_ready`.
  bool has_wrapped_pin() const;
  // Returns true if the wrapped PIN is arbitrary. I.e. is a general
  // alphanumeric string. If false then the wrapped PIN is a 6-digit numeric
  // string. Requires `has_wrapped_pin` to be true.
  bool wrapped_pin_is_arbitrary() const;
  // Returns a copy of the wrapped PIN for passing to `MakeClaimedPINSlowly`.
  // Requires `has_wrapped_pin`.
  std::unique_ptr<webauthn_pb::EnclaveLocalState_WrappedPIN> GetWrappedPIN();

  // Enumerates the types of user verifying signing keys that the EnclaveManager
  // might have for the currently signed-in user.
  enum class UvKeyState {
    // No UV key present; perform user verification using a PIN.
    kNone,
    // A UV key is present and `UserVerifyingKeySigningCallback` will return a
    // signing callback where the UI is handled by the system.
    kUsesSystemUI,
    // A UV key has not yet been created but can be.
    // `UserVerifyingKeyCreationCallback` will return a callback that creates
    // the UV key.
    kUsesSystemUIDeferredCreation,
    // A UV key is present and `UserVerifyingKeySigningCallback` will return a
    // valid callback. However, Chrome UI needs to be shown in order to collect
    // biometrics.
    kUsesChromeUI,
  };
  UvKeyState uv_key_state(bool platform_has_biometrics) const;

  // Calls the given callback with `true` if the current platform supports
  // making user-verifying keys.
  static void AreUserVerifyingKeysSupported(Callback callback);

  // Get an access token for contacting the enclave.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> GetAccessToken(
      base::OnceCallback<void(std::optional<std::string>)> callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // This function is called by the MagicArch integration when the user
  // successfully completes recovery.
  void StoreKeys(const std::string& gaia_id,
                 std::vector<std::vector<uint8_t>> keys,
                 int last_key_version);

  // Slowly compute a PIN claim for the given PIN for submission to the enclave.
  static std::unique_ptr<device::enclave::ClaimedPIN> MakeClaimedPINSlowly(
      std::string pin,
      std::unique_ptr<webauthn_pb::EnclaveLocalState_WrappedPIN> wrapped_pin);

  // If background processes need to be stopped then return true and call
  // `on_stop` when stopped. Otherwise return false.
  bool RunWhenStoppedForTesting(base::OnceClosure on_stop);

  webauthn_pb::EnclaveLocalState& local_state_for_testing() const;

  // Release the cached HW and UV key references.
  void ClearCachedKeysForTesting();

  // Reset the EnclaveManager to simulate creating a new one in initialized
  // state.
  void ResetForTesting();

  // Clears the registration as if we were starting from scratch.
  void ClearRegistrationForTesting();

  // Toggle invariant checks.
  static void EnableInvariantChecksForTesting(bool enable);

  unsigned renewal_checks_for_testing() const;
  unsigned renewal_attempts_for_testing() const;

  // Create a wrapped PIN, suitable for putting into a simulated security domain
  // member.
  static std::string MakeWrappedPINForTesting(
      base::span<const uint8_t> security_domain_secret,
      std::string_view pin);

  base::WeakPtr<EnclaveManager> GetWeakPtr();

 private:
  class StateMachine;
  class IdentityObserver;
  struct PendingAction;
  friend class StateMachine;

  // Starts a `StateMachine` to process the current request.
  void Act();

  // Is called when reading the state file from disk has completed.
  // (Successfully or otherwise.)
  void LoadComplete(std::optional<std::string> contents);

  // Called when `identity_observer_` reports a change in the signed-in state of
  // the Profile. Also called once the local state has finished loading. In
  // that case `is_post_load` will be false and any "change" in primary
  // identity doesn't cause a reset.
  void HandleIdentityChange(bool is_post_load = false);

  // Called when a `StateMachine` has stopped (or needs to stop).
  void Stopped();

  // Called when the primary user changes and all pending actions are stopped.
  void CancelAllActions();

  // Can be called at any point to serialise the current value of `local_state_`
  // to disk. Only a single write happens at a time. If a write is already
  // happening, the request will be queued. If a request is already queued, this
  // call will replace that queued write.
  void WriteState(webauthn_pb::EnclaveLocalState* new_state);
  void DoWriteState(std::string serialized);
  void WriteStateComplete(bool success);

  // Accessors for the HW and UV keys, invoking the supplied callbacks with
  // the result. These can complete synchronously if the respective key is
  // cached, or will attempt to load them asynchronously otherwise.
  // If the key fails to load, the callback will be invoked with nullptr and
  // the device's enclave registration will be reset.
  void GetIdentityKeyForSignature(
      base::OnceCallback<void(
          scoped_refptr<unexportable_keys::RefCountedUnexportableSigningKey>)>
          callback);
  void GetUserVerifyingKeyForSignature(
      UVKeyOptions options,
      base::OnceCallback<void(
          scoped_refptr<crypto::RefCountedUserVerifyingSigningKey>)> callback);

  // If signing keys are lost or disabled, this can put the enclave registration
  // in an unrecoverable state. In this case the registration state needs to be
  // reset, and can be initiated from scratch.
  void ClearRegistration();

  void UnregisterComplete(Callback callback, bool success);

  // Store the secret that `TakeSecret` will make available.
  void SetSecret(int32_t key_version, base::span<const uint8_t> secret);

  // Check whether the GPM PIN Vault should be renewed.
  void ConsiderPinRenewal();
  void OnRenewalComplete(bool success);

  const base::FilePath file_path_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  device::NetworkContextFactory network_context_factory_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::unique_ptr<trusted_vault::TrustedVaultConnection>
      trusted_vault_conn_;
  const std::unique_ptr<trusted_vault::TrustedVaultAccessTokenFetcherFrontend>
      trusted_vault_access_token_fetcher_frontend_;
  const std::unique_ptr<trusted_vault::RecoveryKeyStoreConnection>
      recovery_key_store_conn_;

  std::unique_ptr<webauthn_pb::EnclaveLocalState> local_state_;
  bool loading_ = false;
  raw_ptr<const webauthn_pb::EnclaveLocalState_User> user_ = nullptr;
  std::unique_ptr<CoreAccountInfo> primary_account_info_;
  std::unique_ptr<IdentityObserver> identity_observer_;

  std::optional<std::string> pending_write_;
  bool currently_writing_ = false;
  base::OnceClosure write_finished_callback_;

  std::unique_ptr<StoreKeysArgs> pending_keys_;
  std::unique_ptr<StateMachine> state_machine_;
  std::vector<base::OnceClosure> load_callbacks_;
  std::deque<std::unique_ptr<PendingAction>> pending_actions_;
  base::RepeatingTimer renewal_timer_;
  unsigned renewal_checks_ = 0;
  unsigned renewal_attempts_ = 0;
  bool is_renewing_ = false;

  // These fields store the security domain secret immediately after a
  // device has been added to the security domain.
  int32_t secret_version_ = -1;
  std::vector<uint8_t> secret_;

  // Allow keys to persist across sequences because loading them is slow.
  scoped_refptr<crypto::RefCountedUserVerifyingSigningKey> user_verifying_key_;
  scoped_refptr<unexportable_keys::RefCountedUnexportableSigningKey>
      identity_key_;

  unsigned store_keys_count_ = 0;

  base::ObserverList<Observer> observer_list_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<EnclaveManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_H_
