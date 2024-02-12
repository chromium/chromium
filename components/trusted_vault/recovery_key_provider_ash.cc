// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/recovery_key_provider_ash.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/recoverable_key_store.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_impl.h"
#include "components/trusted_vault/trusted_vault_request.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace trusted_vault {

// Currently only a single application key (for recovering the passkeys security
// domain) is supported.
constexpr char kApplicationKeyName[] =
    "security_domain_member_key_encrypted_locally";

RecoveryKeyProviderAsh::RecoveryKeyProviderAsh(
    scoped_refptr<base::SequencedTaskRunner> user_data_auth_client_task_runner,
    AccountId account_id,
    std::string device_id)
    : user_data_auth_client_task_runner_(user_data_auth_client_task_runner),
      account_id_(std::move(account_id)),
      device_id_(std::move(device_id)) {
  CHECK(user_data_auth_client_task_runner_);
}

RecoveryKeyProviderAsh::~RecoveryKeyProviderAsh() = default;

void RecoveryKeyProviderAsh::GetCurrentRecoveryKeyStoreData(
    RecoveryKeyStoreDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto completion_callback_on_current_sequence =
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&RecoveryKeyProviderAsh::OnUserDataAuthClientAvailable,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
  user_data_auth_client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ash::UserDataAuthClient::WaitForServiceToBeAvailable,
                     base::Unretained(ash::UserDataAuthClient::Get()),
                     std::move(completion_callback_on_current_sequence)));
}

void RecoveryKeyProviderAsh::OnUserDataAuthClientAvailable(
    RecoveryKeyStoreDataCallback callback,
    bool is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_available) {
    // We can't recover if cryptohome is unavailable since there is no recovery
    // factor to upload.
    std::move(callback).Run(std::nullopt);
    return;
  }

  user_data_auth::GetRecoverableKeyStoresRequest request;
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_);

  auto completion_callback_on_current_sequence =
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &RecoveryKeyProviderAsh::OnGetRecoverableKeyStoresReply,
          weak_factory_.GetWeakPtr(), std::move(callback)));
  user_data_auth_client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ash::UserDataAuthClient::GetRecoverableKeyStores,
                     base::Unretained(ash::UserDataAuthClient::Get()),
                     std::move(request),
                     std::move(completion_callback_on_current_sequence)));
}

void RecoveryKeyProviderAsh::OnGetRecoverableKeyStoresReply(
    RecoveryKeyStoreDataCallback callback,
    std::optional<user_data_auth::GetRecoverableKeyStoresReply> reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!reply || reply->error()) {
    DVLOG(1) << "Invalid GetRecoverableKeyStoresReply "
             << (reply ? static_cast<int>(reply->error()) : -1);
    std::move(callback).Run(std::nullopt);
    return;
  }

  // We can only upload a single recovery factor. Pick the PIN if available, and
  // device password otherwise.
  auto chosen_key_store = base::ranges::find_if(
      reply->key_stores(), [](const ::cryptohome::RecoverableKeyStore& pb) {
        return pb.key_store_metadata().knowledge_factor_type() ==
               cryptohome::KNOWLEDGE_FACTOR_TYPE_PIN;
      });
  if (chosen_key_store == reply->key_stores().end()) {
    chosen_key_store = base::ranges::find_if(
        reply->key_stores(), [](const ::cryptohome::RecoverableKeyStore& pb) {
          return pb.key_store_metadata().knowledge_factor_type() ==
                 cryptohome::KNOWLEDGE_FACTOR_TYPE_PASSWORD;
        });
  }

  if (chosen_key_store == reply->key_stores().end()) {
    DVLOG(1) << "No applicable key store";
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (chosen_key_store->wrapped_security_domain_key().key_name() !=
      kApplicationKeyName) {
    // Invalid response from cryptohome.
    DVLOG(1) << "No matching application key";
    std::move(callback).Run(std::nullopt);
    return;
  }

  trusted_vault_pb::UpdateVaultRequest update_vault_request;
  trusted_vault_pb::Vault* vault = update_vault_request.mutable_vault();
  vault->mutable_vault_parameters()->set_backend_public_key(
      chosen_key_store->key_store_parameters().backend_public_key());
  vault->mutable_vault_parameters()->set_counter_id(
      chosen_key_store->key_store_parameters().counter_id());
  vault->mutable_vault_parameters()->set_max_attempts(
      chosen_key_store->key_store_parameters().max_attempts());
  vault->mutable_vault_parameters()->set_vault_handle(
      chosen_key_store->key_store_parameters().key_store_handle());
  chosen_key_store->key_store_parameters().SerializeToString(
      vault->mutable_vault_metadata());
  vault->set_recovery_key(chosen_key_store->wrapped_recovery_key());
  const cryptohome::WrappedSecurityDomainKey& wrapped_security_domain_key =
      chosen_key_store->wrapped_security_domain_key();
  trusted_vault_pb::ApplicationKey* application_key =
      vault->add_application_keys();
  application_key->set_key_name(wrapped_security_domain_key.key_name());
  application_key->mutable_asymmetric_key_pair()->set_public_key(
      wrapped_security_domain_key.public_key());
  application_key->mutable_asymmetric_key_pair()->set_wrapped_private_key(
      wrapped_security_domain_key.wrapped_private_key());
  application_key->mutable_asymmetric_key_pair()->set_wrapping_key(
      wrapped_security_domain_key.wrapped_wrapping_key());

  trusted_vault_pb::ChromeOsMetadata* chrome_os_metadata =
      update_vault_request.mutable_chrome_os_metadata();
  chrome_os_metadata->set_device_id(device_id_);
  chrome_os_metadata->set_chrome_os_version(
      base::SysInfo::OperatingSystemVersion());

  std::move(callback).Run(std::move(update_vault_request));
}

}  // namespace trusted_vault
