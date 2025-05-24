// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_degraded_recoverability_handler.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_throttling_connection.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class AccountsInCookieJarInfo;
}  // namespace signin

namespace trusted_vault {

// Provides interfaces to store/remove keys to/from file storage.
// This class performs expensive operations and expected to be run from
// dedicated sequence (using thread pool). Can be constructed on any thread/
// sequence.
class StandaloneTrustedVaultBackend
    : public base::RefCountedThreadSafe<StandaloneTrustedVaultBackend>,
      public TrustedVaultDegradedRecoverabilityHandler::Delegate {
 public:
  using FetchKeysCallback = base::OnceCallback<void(
      const std::vector<std::vector<uint8_t>>& vault_keys)>;

  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    virtual ~Delegate() = default;

    Delegate& operator=(const Delegate&) = delete;

    virtual void NotifyRecoverabilityDegradedChanged() = 0;
  };

  class LocalRecoveryFactorsFactory {
   public:
    LocalRecoveryFactorsFactory() = default;
    LocalRecoveryFactorsFactory(const LocalRecoveryFactorsFactory&) = delete;
    virtual ~LocalRecoveryFactorsFactory() = default;

    LocalRecoveryFactorsFactory& operator=(const LocalRecoveryFactorsFactory&) =
        delete;

    // Creates LocalRecoveryFactor's for |primary_account|.
    // Note that the returned LocalRecoveryFactor's will keep a reference to
    // |storage| and |connection|.
    virtual std::vector<std::unique_ptr<LocalRecoveryFactor>>
    CreateLocalRecoveryFactors(SecurityDomainId security_domain_id,
                               StandaloneTrustedVaultStorage* storage,
                               TrustedVaultThrottlingConnection* connection,
                               const CoreAccountInfo& primary_account) = 0;
  };

  enum class RefreshTokenErrorState {
    // State can not be identified (e.g. refresh token is not loaded yet).
    kUnknown,
    // Refresh token is in persistent auth error state.
    kPersistentAuthError,
    // There are no persistent auth errors (note, that transient errors are
    // still possible).
    kNoPersistentAuthErrors,
  };

  // |connection| can be null, in this case functionality that involves
  // interaction with vault service (such as recovery factor registration, keys
  // downloading, etc.) will be disabled.
  StandaloneTrustedVaultBackend(
#if BUILDFLAG(IS_MAC)
      const std::string& icloud_keychain_access_group_prefix,
#endif
      SecurityDomainId security_domain_id,
      std::unique_ptr<StandaloneTrustedVaultStorage> storage,
      std::unique_ptr<Delegate> delegate,
      std::unique_ptr<TrustedVaultConnection> connection);
  StandaloneTrustedVaultBackend(const StandaloneTrustedVaultBackend& other) =
      delete;
  StandaloneTrustedVaultBackend& operator=(
      const StandaloneTrustedVaultBackend& other) = delete;

  // TrustedVaultDegradedRecoverabilityHandler::Delegate implementation.
  void WriteDegradedRecoverabilityState(
      const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
          degraded_recoverability_state) override;
  void OnDegradedRecoverabilityChanged() override;

  // Restores state saved on disk, should be called before using the object.
  void ReadDataFromDisk();

  // Populates vault keys corresponding to |account_info| into |callback|. If
  // recent keys are locally available, |callback| will be called immediately.
  // Otherwise, attempts to download new keys from the server. In case of
  // failure or if current state isn't sufficient it will populate locally
  // available keys regardless of their freshness.
  void FetchKeys(const CoreAccountInfo& account_info,
                 FetchKeysCallback callback);

  // Replaces keys for given |gaia_id| both in memory and on disk.
  void StoreKeys(const GaiaId& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version);

  // Marks vault keys as stale.  Afterwards, the next FetchKeys() call for this
  // |account_info| will trigger a key download attempt.
  bool MarkLocalKeysAsStale(const CoreAccountInfo& account_info);

  // Sets/resets |primary_account_|.
  void SetPrimaryAccount(const std::optional<CoreAccountInfo>& primary_account,
                         RefreshTokenErrorState refresh_token_error_state);

  // Handles changes of accounts in cookie jar and removes keys for some
  // accounts:
  // 1. Non-primary account keys are removed if account isn't in cookie jar.
  // 2. Primary account keys marked for deferred deletion if account isn't in
  // cookie jar.
  void UpdateAccountsInCookieJarInfo(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info);

  // Returns whether recoverability of the keys is degraded and user action is
  // required to add a new method.
  void GetIsRecoverabilityDegraded(const CoreAccountInfo& account_info,
                                   base::OnceCallback<void(bool)> cb);

  // Registers a new trusted recovery method that can be used to retrieve keys.
  void AddTrustedRecoveryMethod(const GaiaId& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceClosure cb);

  void ClearLocalDataForAccount(const CoreAccountInfo& account_info);

  std::optional<CoreAccountInfo> GetPrimaryAccountForTesting() const;

  trusted_vault_pb::LocalDeviceRegistrationInfo
  GetDeviceRegistrationInfoForTesting(const GaiaId& gaia_id);

  std::vector<uint8_t> GetLastAddedRecoveryMethodPublicKeyForTesting() const;
  int GetLastKeyVersionForTesting(const GaiaId& gaia_id);

  bool HasPendingTrustedRecoveryMethodForTesting() const;

  static scoped_refptr<StandaloneTrustedVaultBackend> CreateForTesting(
      SecurityDomainId security_domain_id,
      std::unique_ptr<StandaloneTrustedVaultStorage> storage,
      std::unique_ptr<Delegate> delegate,
      std::unique_ptr<TrustedVaultThrottlingConnection> connection,
      std::unique_ptr<LocalRecoveryFactorsFactory>
          local_recovery_factors_factory);

 private:
  friend class base::RefCountedThreadSafe<StandaloneTrustedVaultBackend>;

  // Constructor which allows specifying a TrustedVaultThrottlingConnection and
  // a LocalRecoveryFactorsFactory.
  // Only used in tests.
  StandaloneTrustedVaultBackend(
      SecurityDomainId security_domain_id,
      std::unique_ptr<StandaloneTrustedVaultStorage> storage,
      std::unique_ptr<Delegate> delegate,
      std::unique_ptr<TrustedVaultThrottlingConnection> connection,
      std::unique_ptr<LocalRecoveryFactorsFactory>
          local_recovery_factors_factory);

  static TrustedVaultDownloadKeysStatusForUMA
  GetDownloadKeysStatusForUMAFromResponse(
      TrustedVaultDownloadKeysStatus response_status);

  ~StandaloneTrustedVaultBackend() override;

  // Attempts to register local recovery factors in case they're not yet
  // registered and currently available local data is sufficient to do it. Also
  // records registration related metrics.
  void MaybeRegisterLocalRecoveryFactors();

  // Attempts to honor the pending operation stored in
  // |pending_trusted_recovery_method_|.
  void MaybeProcessPendingTrustedRecoveryMethod();

  // Called when registration of a local recovery factor for |gaia_id| is
  // completed (either successfully or not). |storage_| must contain
  // LocalTrustedVaultPerUser for given |gaia_id|.
  void OnRecoveryFactorRegistered(
      LocalRecoveryFactorType local_recovery_factor_type,
      TrustedVaultRegistrationStatus status,
      int key_version,
      bool had_local_keys);

  void AttemptRecoveryFactor(size_t local_recovery_factor);
  void OnKeysRecovered(size_t current_local_recovery_factor,
                       LocalRecoveryFactor::RecoveryStatus status,
                       const std::vector<std::vector<uint8_t>>& new_vault_keys,
                       int last_vault_key_version);

  void OnTrustedRecoveryMethodAdded(base::OnceClosure cb);

  // Invokes |callback| with currently available keys for |gaia_id|.
  void FulfillFetchKeys(
      const GaiaId& gaia_id,
      FetchKeysCallback callback,
      std::optional<TrustedVaultRecoverKeysOutcomeForUMA> status_for_uma);

  // Same as above, but takes parameters from |ongoing_fetch_keys|, used when
  // keys are fetched asynchronously, after keys downloading attempt.
  void FulfillOngoingFetchKeys(
      std::optional<TrustedVaultRecoverKeysOutcomeForUMA> status_for_uma);

  // Removes all data for non-primary accounts if they were previously marked
  // for deletion due to accounts in cookie jar changes.
  void RemoveNonPrimaryAccountKeysIfMarkedForDeletion();

  const SecurityDomainId security_domain_id_;

  const std::unique_ptr<StandaloneTrustedVaultStorage> storage_;

  const std::unique_ptr<Delegate> delegate_;

  // Used for communication with trusted vault server. Can be null, in this case
  // functionality that involves interaction with vault service (such as
  // recovery factor registration, keys downloading, etc.) will be disabled.
  // Note: |connection_| depends on |storage_|, so it needs to be destroyed
  // first. Thus, the field order matters.
  // TODO(crbug.com/40143544): |connection_| can be null if URL passed as
  // kTrustedVaultServiceURLSwitch is not valid, consider making it non-nullable
  // even in this case and clean up related logic.
  const std::unique_ptr<TrustedVaultThrottlingConnection> connection_;

  // Only current |primary_account_| can be used for communication with trusted
  // vault server.
  std::optional<CoreAccountInfo> primary_account_;

  // Factory to create |local_recovery_factors_|. Can be overwritten in tests.
  const std::unique_ptr<LocalRecoveryFactorsFactory>
      local_recovery_factors_factory_;
  // All known local recovery factors that can be used to attempt key recovery.
  // Note: |local_recovery_factors_| depends on |storage_|, thus it must be
  // destroyed before |storage_| (i.e. the order of the fields matters).
  std::vector<std::unique_ptr<LocalRecoveryFactor>> local_recovery_factors_;

  // Error state of refresh token for |primary_account_|.
  RefreshTokenErrorState refresh_token_error_state_ =
      StandaloneTrustedVaultBackend::RefreshTokenErrorState::kUnknown;

  // If AddTrustedRecoveryMethod() gets invoked before SetPrimaryAccount(), the
  // execution gets deferred until SetPrimaryAccount() is invoked.
  struct PendingTrustedRecoveryMethod {
    PendingTrustedRecoveryMethod();
    PendingTrustedRecoveryMethod(PendingTrustedRecoveryMethod&) = delete;
    PendingTrustedRecoveryMethod& operator=(PendingTrustedRecoveryMethod&) =
        delete;
    PendingTrustedRecoveryMethod(PendingTrustedRecoveryMethod&&);
    PendingTrustedRecoveryMethod& operator=(PendingTrustedRecoveryMethod&&);
    ~PendingTrustedRecoveryMethod();

    GaiaId gaia_id;
    std::vector<uint8_t> public_key;
    int method_type_hint;
    base::OnceClosure completion_callback;
  };
  std::optional<PendingTrustedRecoveryMethod> pending_trusted_recovery_method_;

  // Keys fetching is asynchronous when it involves sending request to the
  // server, this structure encapsulates the data needed to process the response
  // and allow concurrent key fetches for the same user. Destroying this will
  // cancel the ongoing request.
  // Note, that |gaia_id| should match |primary_account_|. It is used only for
  // verification.
  struct OngoingFetchKeys {
    OngoingFetchKeys();
    OngoingFetchKeys(OngoingFetchKeys&) = delete;
    OngoingFetchKeys& operator=(OngoingFetchKeys&) = delete;
    OngoingFetchKeys(OngoingFetchKeys&&);
    OngoingFetchKeys& operator=(OngoingFetchKeys&&);
    ~OngoingFetchKeys();

    GaiaId gaia_id;
    std::vector<FetchKeysCallback> callbacks;
  };
  std::optional<OngoingFetchKeys> ongoing_fetch_keys_;

  // Same as above, but specifically used for recoverability-related requests.
  // TODO(crbug.com/40178774): Move elsewhere.
  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_add_recovery_method_request_;

  // Used to take care of polling the degraded recoverability state from the
  // server for the |primary_account|. Instance changes whenever
  // |primary_account| changes.
  std::unique_ptr<TrustedVaultDegradedRecoverabilityHandler>
      degraded_recoverability_handler_;

  std::vector<uint8_t> last_added_recovery_method_public_key_for_testing_;

  bool recovery_factor_registration_state_recorded_to_uma_ = false;

  // If GetIsRecoverabilityDegraded() gets invoked before
  // SetPrimaryAccount(), the execution gets deferred until
  // SetPrimaryAccount() is invoked. This is possible because
  // SetPrimaryAccount() is called only once refresh token are loaded and
  // GetIsRecoverabilityDegraded() could be invoked before that.
  struct PendingGetIsRecoverabilityDegraded {
    PendingGetIsRecoverabilityDegraded();
    PendingGetIsRecoverabilityDegraded(PendingGetIsRecoverabilityDegraded&) =
        delete;
    PendingGetIsRecoverabilityDegraded& operator=(
        PendingGetIsRecoverabilityDegraded&) = delete;
    PendingGetIsRecoverabilityDegraded(PendingGetIsRecoverabilityDegraded&&);
    PendingGetIsRecoverabilityDegraded& operator=(
        PendingGetIsRecoverabilityDegraded&&);
    ~PendingGetIsRecoverabilityDegraded();

    CoreAccountInfo account_info;
    base::OnceCallback<void(bool)> completion_callback;
  };
  std::optional<PendingGetIsRecoverabilityDegraded>
      pending_get_is_recoverability_degraded_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_
