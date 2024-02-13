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
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"

namespace trusted_vault {

// RecoveryKeyStoreController uploads application keys to the recovery key
// store service.
class RecoveryKeyStoreController {
 public:
  // ApplicationKey describes a single key pair that was successfully uploaded
  // to the recovery key store. This typically is a virtual device member of a
  // security domain.
  struct ApplicationKey {
    ApplicationKey(std::string name, std::vector<uint8_t> public_key);
    ApplicationKey(ApplicationKey&);
    ApplicationKey(ApplicationKey&&);
    ApplicationKey& operator=(ApplicationKey&);
    ApplicationKey& operator=(ApplicationKey&&);
    ~ApplicationKey();

    std::string name;
    std::vector<uint8_t> public_key;
  };

  // The RecoveryKeyProvider is responsible for assembling platform-specific
  // data to be uploaded to the recovery key store service.
  class RecoveryKeyProvider {
   public:
    using RecoveryKeyStoreDataCallback = base::OnceCallback<void(
        std::optional<trusted_vault_pb::UpdateVaultRequest>)>;

    virtual ~RecoveryKeyProvider();

    virtual void GetCurrentRecoveryKeyStoreData(
        RecoveryKeyStoreDataCallback) = 0;
  };

  // The observer interface lets implementers receive application keys after
  // they were uploaded successfully.
  class Observer {
   public:
    // Invoked whenever an attempt to upload to recovery key store completes
    // successfully.
    virtual void OnUpdateRecoveryKeyStore(
        const std::vector<ApplicationKey>& application_keys) = 0;
  };

  // `recovery_key_provider`, `connection` and `observer` must not be null.
  // `observer` must outlive `this`.
  RecoveryKeyStoreController(
      CoreAccountInfo account_info,
      std::unique_ptr<RecoveryKeyProvider> recovery_key_provider,
      std::unique_ptr<RecoveryKeyStoreConnection> connection,
      Observer* observer,
      base::Time last_update,
      base::TimeDelta update_period);

  RecoveryKeyStoreController(const RecoveryKeyStoreController&) = delete;

  RecoveryKeyStoreController& operator=(const RecoveryKeyStoreController&) =
      delete;

  ~RecoveryKeyStoreController();

 private:
  struct OngoingUpdate {
    OngoingUpdate();
    OngoingUpdate(OngoingUpdate&&);
    OngoingUpdate& operator=(OngoingUpdate&&);
    ~OngoingUpdate();

    std::unique_ptr<RecoveryKeyStoreConnection::Request> request;
  };

  void ScheduleNextUpdate(base::TimeDelta delay);
  void UpdateRecoveryKeyStore();
  void OnGetCurrentRecoveryKeyStoreData(
      std::optional<trusted_vault_pb::UpdateVaultRequest> request);
  void OnUpdateRecoveryKeyStore(std::vector<ApplicationKey> application_keys,
                                UpdateRecoveryKeyStoreStatus status);
  void CompleteUpdateRequest(const std::vector<ApplicationKey>& result);

  const CoreAccountInfo account_info_;
  std::unique_ptr<RecoveryKeyProvider> recovery_key_provider_;
  std::unique_ptr<RecoveryKeyStoreConnection> connection_;
  raw_ptr<Observer> observer_;

  const base::TimeDelta update_period_;
  base::OneShotTimer next_update_timer_;

  std::optional<OngoingUpdate> ongoing_update_;

  base::WeakPtrFactory<RecoveryKeyStoreController> weak_factory_{this};
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONTROLLER_H_
