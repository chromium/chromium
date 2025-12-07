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
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "chrome/browser/webauthn/local_authentication_token.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "content/public/browser/global_routing_id.h"
#include "crypto/user_verifying_key.h"
#include "device/fido/enclave/types.h"
#include "device/fido/network_context_factory.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/common/chrome_version.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
#include <variant>
#endif  // BUILDFLAG(IS_CHROMEOS)

class GaiaId;

namespace base {
class ElapsedTimer;
}

namespace crypto {
class RefCountedUserVerifyingSigningKey;
}  // namespace crypto

#if BUILDFLAG(IS_CHROMEOS)
namespace ash {
class WebAuthNDialogController;
class ActiveSessionAuthController;
}  // namespace ash
#endif

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

#if BUILDFLAG(IS_MAC)
class ICloudRecoveryKey;
#endif  // BUILDFLAG(IS_MAC)
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

#if BUILDFLAG(IS_CHROMEOS)
    std::variant<raw_ptr<ash::WebAuthNDialogController>,
                 raw_ptr<ash::ActiveSessionAuthController>>
        dialog_controller;
#endif

    // An optional auth context. Currently only used to pass LAcontext to Apple
    // Keychain operations.
    std::optional<webauthn::LocalAuthenticationToken> local_auth_token;
  };

  // These values are detailed failure reasons. They are emitted whenever PIN
  // renewal fails and give detailed information about why the attempt failed.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(PinRenewalFailureCause)
  enum class PinRenewalFailureCause {
    kDuringDownload = 1,
    kGettingAccessToken = 2,
    kEnclaveRequest1 = 3,
    kEnclaveRequest2 = 4,
    kEnclaveResponse1 = 5,
    kEnclaveResponse2 = 6,
    kRKSUpload = 7,
    kJoiningToDomain = 8,
    kSecurityDomainReportsNoPin = 9,
    kSecurityDomainReset = 10,
    kCohortNotYetDeprecated = 11,
    kRecoveryKeyStoreDowngrade = 12,

    kMaxValue = kRecoveryKeyStoreDowngrade,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:WebAuthenticationPinRenewalFailureCause)

  class UvKeyCreationLock {
   public:
    virtual ~UvKeyCreationLock() = default;
    UvKeyCreationLock(const UvKeyCreationLock&) = delete;
    UvKeyCreationLock& operator=(const UvKeyCreationLock&) = delete;

   protected:
    UvKeyCreationLock() = default;
  };

  // A reference to this object is returned to represent a claim on key provided
  // by accounts.google.com. See `GetStoreKeysLock`.
  class StoreKeysLock {
   public:
    explicit StoreKeysLock(base::WeakPtr<EnclaveManager> manager);
    StoreKeysLock(const StoreKeysLock&) = delete;
    StoreKeysLock(StoreKeysLock&&) = delete;
    StoreKeysLock& operator=(const StoreKeysLock&) = delete;
    StoreKeysLock& operator=(StoreKeysLock&&) = delete;
    ~StoreKeysLock();

   private:
    const base::WeakPtr<EnclaveManager> manager_;
    SEQUENCE_CHECKER(sequence_checker_);
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
  bool is_loaded() const override;
  // Returns true if the current user has been registered with the enclave.
  bool is_registered() const override;
  // Returns true if `StoreKeys` has been called and thus `AddDeviceToAccount`
  // or `AddDeviceAndPINToAccount` can be called.
  bool has_pending_keys() const;
  // Returns true if the current user has joined the security domain and has one
  // or more wrapped security domain secrets available. (This implies
  // `is_registered`.)
  bool is_ready() const override;
  // Returns the number of times that `StoreKeys` has been called.
  unsigned store_keys_count() const;

  // Load the persisted state from disk. Harmless to call if `is_loaded`.
  void Load(base::OnceClosure closure);
  // Preforms `Load` after the given delay,
  void LoadAfterDelay(base::TimeDelta delay,
                      base::OnceClosure closure) override;
  // Register with the enclave if not already registered.
  void RegisterIfNeeded(Callback callback);
  // Set up an account with a newly-created PIN.
  void SetupWithPIN(std::string pin, Callback callback);
  // Take a lock that prevents any keys provided by accounts.google.com from
  // being opportunistically used to register with the enclave. While a
  // `StoreKeysLock` object exists, any stored keys will wait for a call to,
  // e.g. `AddDeviceToAccount`. The lock only needs to span the `StoreKeys`
  // call, it doesn't need to be held throughout adding the device to the
  // security domain.
  std::unique_ptr<StoreKeysLock> GetStoreKeysLock();
  // Adds the current device to the security domain. This method is supposed to
  // be called after calling `StoreKeys` (with a lock outstanding from
  // `GetStoreKeysLock`) and thus `has_pending_keys` returns true. Also, this
  // method is being called from `StoreKeysFromOutOfContextRetrieval`.
  //
  // If `pin_metadata` has a value then it is taken to be the current GPM PIN.
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
  // `previous_pin_public_key` must be set if the PIN is replacing an existing
  // GPM PIN.
  void AddDeviceAndPINToAccount(
      std::string pin,
      std::optional<std::string> previous_pin_public_key,
      Callback callback);
  // Set a PIN on an account that doesn't currently have one.
  void SetPIN(std::string pin, std::string rapt, Callback callback);
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
      std::unique_ptr<trusted_vault::ICloudRecoveryKey> icloud_recovery_key,
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
  // The `UVKeyCreationLock` prevents any other attempts to create UV keys
  // while it is alive. Its destruction releases the lock.
  std::pair<std::unique_ptr<UvKeyCreationLock>,
            device::enclave::UVKeyCreationCallback>
  UserVerifyingKeyCreationCallback();
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
  // Replaces the wrapped PIN data.
  // Requires `has_wrapped_pin`.
  void SetWrappedPINDataForTesting(std::vector<uint8_t> wrapped_pin_data);
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

  void CheckGpmPinAvailability(GpmPinAvailabilityCallback callback) override;

  // Checks whether UserVerifyingKeyCreationCallback() is available to be
  // called, returning true if not. There should only be one key creation
  // callback in existence at any one time, or else one could overwrite a
  // previous caller's keys. Attempting to get a key creation callback
  // while already locked results in a process crash.
  bool deferred_uv_key_creation_locked() const {
    return deferred_uv_key_creation_in_progress_;
  }

  // Called when `deferred_uv_key_creation_in_progress_` is true, to be
  // notified when the existing key creation has completed. The boolean
  // argument indicates whether the key creation was successful.
  void AddPendingUvRequest(base::OnceCallback<void(bool)> callback);

  // Calls the given callback with `true` if the current platform supports
  // making user-verifying keys.
  static void AreUserVerifyingKeysSupported(Callback callback);

  // Get an access token for contacting the enclave.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> GetAccessToken(
      base::OnceCallback<void(std::optional<std::string>)> callback);

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // This function is called by the MagicArch integration when the user
  // successfully completes recovery. It must be called either with a lock
  // outstanding from `GetStoreKeysLock`, or without a lock (but in this case
  // the keys will be stored only if a system UV is available).
  void StoreKeys(const GaiaId& gaia_id,
                 std::vector<std::vector<uint8_t>> keys,
                 int last_key_version);

  // Slowly compute a PIN claim for the given PIN for submission to the enclave.
  static std::unique_ptr<device::enclave::ClaimedPIN> MakeClaimedPINSlowly(
      std::string pin,
      std::unique_ptr<webauthn_pb::EnclaveLocalState_WrappedPIN> wrapped_pin);

  // If background processes need to be stopped then return true and call
  // `on_stop` when stopped. Otherwise return false.
  bool RunWhenStoppedForTesting(base::OnceClosure on_stop);

  webauthn_pb::EnclaveLocalState& local_state_for_testing();

  // Release the cached HW and UV key references.
  void ClearCachedKeysForTesting();

  // Reset the EnclaveManager to simulate creating a new one in initialized
  // state.
  void ResetForTesting();

  // Clears the registration as if we were starting from scratch.
  void ClearRegistrationForTesting();

  // Toggle invariant checks.
  static void EnableInvariantChecksForTesting(bool enable);

  // Check whether the GPM PIN Vault should be renewed, and do so if needed.
  void ConsiderPinRenewalForTesting();

  void NotifyObserversThatStateUpdated();

  unsigned renewal_checks_for_testing() const;
  unsigned renewal_attempts_for_testing() const;

  // Create a wrapped PIN, suitable for putting into a simulated security domain
  // member.
  static std::string MakeWrappedPINForTesting(
      base::span<const uint8_t> security_domain_secret,
      std::string_view pin);

  // Encrypts `cbor_bytes` representing a wrapped PIN with
  // `security_domain_secret`.
  static std::vector<uint8_t> EncryptWrappedPIN(
      base::span<const uint8_t> security_domain_secret,
      base::span<const uint8_t> cbor_bytes);

  base::WeakPtr<EnclaveManager> GetWeakPtr();

 private:
  class StateMachine;
  class IdentityObserver;
  struct PendingAction;
  friend class StateMachine;
  friend class StoreKeysLock;
  FRIEND_TEST_ALL_PREFIXES(EnclaveUVTest, UnregisterOnMissingUserVerifyingKey);

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

  // Take the lock for UV key creation. Only one can exist at a time.
  std::unique_ptr<UvKeyCreationLock> TakeUvKeyCreationLock();

  // This is a callback for the UvKeyCreationLock.
  void OnUvKeyCreationLockReleased();

  // These clean up local state on resolution of a callback that was returned
  // from UserVerifyingKeyCreationCallback();
  void OnDeferredUvKeyCreationFailure();
  void OnDeferredUvKeyCreationSuccess();

  // Returns true if |state| indicates that the security domain has been reset,
  // i.e. that the local Chrome state no longer matches what's on the security
  // domain.
  bool IsSecurityDomainReset(
      const trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult&
          state);

  // Called when the OSCrypt encryptor is available.
  void OnOsCryptReady(os_crypt_async::Encryptor encryptor);

  // Called when the result of checking the GPM PIN availability is received.
  void OnCheckGpmPinAvailabilityResult(
      base::OnceCallback<void(GpmPinAvailability)> callback,
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
          result);

  // Stores keys in the pending state (the keys will remain in this state until
  // `AddDeviceToAccount` is called).
  void StorePendingKeys(const GaiaId& gaia_id,
                        std::vector<std::vector<uint8_t>> keys,
                        int last_key_version);

  // Stores keys and performs `AddDeviceToAccount` if the system UV or the GPM
  // PIN is available.
  void StoreKeysFromOutOfContextRetrieval(
      const GaiaId& gaia_id,
      std::vector<std::vector<uint8_t>> keys,
      int last_key_version);
  // Used by `StoreKeysFromOutOfContextRetrieval`. Executed upon verification of
  // the system UV availability. If a system UV is available - stores the
  // opportunistically retrieved keys. If a system UV is not available -
  // starts verification of the presence of a GPM PIN (because the GPM PIN can
  // be used for user verification as well). If the GPM PIN is present - the
  // opportunistically retrieved keys will be stored as well.
  void OpportunisticStoreKeysUVCheckComplete(
      std::unique_ptr<StoreKeysArgs> pending_keys,
      bool can_make_uv_keys);
  // Indirectly used by `StoreKeysFromOutOfContextRetrieval`
  // (`StoreKeysFromOutOfContextRetrieval` performs the check of the presence of
  // the system UV, and if the system UV is not available - we check the
  // presence of the GPM PIN). This method is being executed upon verification
  // of the GPM PIN availability. If the GPM PIN is present - the
  // opportunistically retrieved keys will be stored.
  void OpportunisticStoreKeysGpmPinCheckComplete(
      std::unique_ptr<StoreKeysArgs> pending_keys,
      GpmPinAvailability gpm_pin_availability);
  // Indirectly used by `StoreKeysFromOutOfContextRetrieval`: if either the
  // system UV is present or the GPM PIN is present - stores keys.
  void OpportunisticStoreKeys(std::unique_ptr<StoreKeysArgs> pending_keys);
  void OpportunisticStoreKeysAddComplete(bool success);
  void NotifyObserversAboutOutOfContextRecoveryOutcome(
      OutOfContextRecoveryOutcome outcome);

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
  base::OneShotTimer load_timer_;
  base::RepeatingTimer renewal_timer_;
  unsigned renewal_checks_ = 0;
  unsigned renewal_attempts_ = 0;
  bool is_renewing_ = false;
  bool deferred_uv_key_creation_in_progress_ = false;
  std::optional<bool> deferred_uv_key_creation_successful_;
  std::vector<base::OnceCallback<void(bool)>> pending_uv_key_requests_;

  // These fields store the security domain secret immediately after a
  // device has been added to the security domain.
  int32_t secret_version_ = -1;
  std::vector<uint8_t> secret_;

  // Allow keys to persist across sequences because loading them is slow.
  scoped_refptr<crypto::RefCountedUserVerifyingSigningKey> user_verifying_key_;
  scoped_refptr<unexportable_keys::RefCountedUnexportableSigningKey>
      identity_key_;

  unsigned store_keys_count_ = 0;
  unsigned store_keys_lock_depth_ = 0;

  // Timer for recording a metric measuring the delay to load the Enclave
  // state.
  std::unique_ptr<base::ElapsedTimer> load_duration_timer_;

  base::ObserverList<Observer> observer_list_;

  std::optional<os_crypt_async::Encryptor> encryptor_;

  std::unique_ptr<trusted_vault::TrustedVaultConnection::Request>
      download_account_state_request_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<EnclaveManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_H_
