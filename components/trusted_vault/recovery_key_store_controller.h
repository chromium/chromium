// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONTROLLER_H_
#define COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONTROLLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"

namespace trusted_vault {

// RecoveryKeyStoreController uploads application keys to the recovery key
// store service.
class RecoveryKeyStoreController {
 public:
  // The RecoveryKeyProvider is responsible for assembling platform-specific
  // data to be uploaded to the recovery key store service.
  class RecoveryKeyProvider {
   public:
    using RecoveryKeyStoreDataCallback =
        base::OnceCallback<void(std::optional<trusted_vault_pb::Vault>)>;

    virtual ~RecoveryKeyProvider();

    virtual void GetCurrentRecoveryKeyStoreData(
        RecoveryKeyStoreDataCallback) = 0;
  };

  // The delegate interface persists recovery key store state to disk.
  class Delegate {
   public:
    using RecoveryKeyRegistrationCallback =
        base::OnceCallback<void(TrustedVaultRegistrationStatus)>;

    // Invoked whenever an attempt to upload to recovery key store completes
    // successfully.
    virtual void WriteRecoveryKeyStoreState(
        const trusted_vault_pb::RecoveryKeyStoreState&) = 0;

    // Invoked to register the public part of a recovery key pair as a member of
    // the security domain.
    virtual void AddRecoveryKeyToSecurityDomain(
        const std::vector<uint8_t>& public_key,
        RecoveryKeyRegistrationCallback callback) = 0;
  };

  static constexpr base::TimeDelta kDefaultUpdatePeriod = base::Hours(23);

  // `recovery_key_provider`, `connection` and `delegate` must not be null.
  // `delegate` must outlive `this`.
  RecoveryKeyStoreController(
      std::unique_ptr<RecoveryKeyProvider> recovery_key_provider,
      std::unique_ptr<RecoveryKeyStoreConnection> connection,
      Delegate* delegate);

  RecoveryKeyStoreController(const RecoveryKeyStoreController&) = delete;

  RecoveryKeyStoreController& operator=(const RecoveryKeyStoreController&) =
      delete;

  ~RecoveryKeyStoreController();

  // Enables periodic uploads to the recovery key store service for the account
  // identified by `account_info`. `state` is the last persisted state by the
  // delegate. `update_period` determines the frequency of future uploads.
  //
  // Any uploads that are already scheduled or in-flight will be stopped. (I.e.,
  // concurrent scheduling for multiple accounts is not implemented.)
  void StartPeriodicUploads(
      CoreAccountInfo account_info,
      const trusted_vault_pb::RecoveryKeyStoreState& state,
      base::TimeDelta update_period);

  // Disables future uploads to the recovery key store service.
  void StopPeriodicUploads();

 private:
  struct OngoingUpdate {
    OngoingUpdate();
    OngoingUpdate(OngoingUpdate&&);
    OngoingUpdate& operator=(OngoingUpdate&&);
    ~OngoingUpdate();

    std::optional<trusted_vault_pb::Vault> current_vault_proto;
    std::unique_ptr<RecoveryKeyStoreConnection::Request> request;
  };

  void ScheduleNextUpdate(base::TimeDelta delay);
  void StartUpdateCycle();
  void OnGetCurrentRecoveryKeyStoreData(
      std::optional<trusted_vault_pb::Vault> request);
  void MaybeAddRecoveryKeyToSecurityDomain();
  void OnRecoveryKeyAddedToSecurityDomain(
      TrustedVaultRegistrationStatus status);
  void UpdateRecoveryKeyStore();
  void OnUpdateRecoveryKeyStore(UpdateRecoveryKeyStoreStatus status);
  void CompleteUpdateCycle();

  std::unique_ptr<RecoveryKeyProvider> recovery_key_provider_;
  std::unique_ptr<RecoveryKeyStoreConnection> connection_;
  raw_ptr<Delegate> delegate_;

  std::optional<CoreAccountInfo> account_info_;
  trusted_vault_pb::RecoveryKeyStoreState state_;
  base::TimeDelta update_period_;
  base::OneShotTimer next_update_timer_;

  std::optional<OngoingUpdate> ongoing_update_;

  base::WeakPtrFactory<RecoveryKeyStoreController> weak_factory_{this};
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONTROLLER_H_
