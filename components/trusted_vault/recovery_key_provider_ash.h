// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_PROVIDER_ASH_H_
#define COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_PROVIDER_ASH_H_

#include <optional>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/recoverable_key_store.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/recovery_key_store_controller.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"

namespace trusted_vault {

// Fetches recovery key store data for ChromeOS devices, which contains a set of
// LSKF-wrapped keys.
class RecoveryKeyProviderAsh
    : public RecoveryKeyStoreController::RecoveryKeyProvider {
 public:
  // `user_data_auth_client_task_runner` must be non-null and able to execute
  // requests to `ash::UserDataAuthClient` (i.e. the main thread task runner).
  RecoveryKeyProviderAsh(scoped_refptr<base::SequencedTaskRunner>
                             user_data_auth_client_task_runner,
                         AccountId account_id,
                         std::string device_id);
  RecoveryKeyProviderAsh(const RecoveryKeyProviderAsh&) = delete;
  RecoveryKeyProviderAsh& operator=(const RecoveryKeyProviderAsh&) = delete;
  ~RecoveryKeyProviderAsh() override;

  void GetCurrentRecoveryKeyStoreData(
      RecoveryKeyStoreDataCallback callback) override;

 private:
  void OnUserDataAuthClientAvailable(RecoveryKeyStoreDataCallback callback,
                                     bool is_available);
  void OnGetRecoverableKeyStoresReply(
      RecoveryKeyStoreDataCallback callback,
      std::optional<user_data_auth::GetRecoverableKeyStoresReply> reply);

  // Used to schedule requests to `ash::UserDataAuthClient`. This must be the
  // main thread task runner.
  scoped_refptr<base::SequencedTaskRunner> user_data_auth_client_task_runner_;

  const AccountId account_id_;
  const std::string device_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RecoveryKeyProviderAsh> weak_factory_{this};
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_PROVIDER_ASH_H_
