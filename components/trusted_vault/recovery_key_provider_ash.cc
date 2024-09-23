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
  // The instance is created on the main thread, but lives on the
  // `StandadloneTrustedVaultBackend` utility thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
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

  trusted_vault_pb::Vault vault;
  vault.mutable_vault_parameters()->set_backend_public_key(
      chosen_key_store->key_store_parameters().backend_public_key());
  vault.mutable_vault_parameters()->set_counter_id(
      chosen_key_store->key_store_parameters().counter_id());
  vault.mutable_vault_parameters()->set_max_attempts(
      chosen_key_store->key_store_parameters().max_attempts());
  vault.mutable_vault_parameters()->set_vault_handle(
      chosen_key_store->key_store_parameters().key_store_handle());

  // The RecoverableKeyStoreMetadata and VaultMetadata protos are not
  // binary-compatible so we need to translate field by field before we can
  // serialize it into the `vault_metadata` proto string field.
  trusted_vault_pb::VaultMetadata vault_metadata;
  const ::cryptohome::RecoverableKeyStoreMetadata& key_store_metadata =
      chosen_key_store->key_store_metadata();
  switch (key_store_metadata.knowledge_factor_type()) {
    case ::cryptohome::KNOWLEDGE_FACTOR_TYPE_PASSWORD:
      vault_metadata.set_lskf_type(
          trusted_vault_pb::VaultMetadata_LskfType_PASSWORD);
      break;
    case ::cryptohome::KNOWLEDGE_FACTOR_TYPE_PIN:
      vault_metadata.set_lskf_type(
          trusted_vault_pb::VaultMetadata_LskfType_PIN);
      break;
    default:
      // Default is necessary because this proto compiles with sentinels such as
      // `INT_MIN_SENTINEL_DO_NOT_USE_`.
      DVLOG(1) << "Unknown knowledge factor type: "
               << static_cast<int>(key_store_metadata.knowledge_factor_type());
      std::move(callback).Run(std::nullopt);
      return;
  }
  switch (key_store_metadata.hash_type()) {
    case ::cryptohome::HASH_TYPE_PBKDF2_AES256_1234:
      vault_metadata.set_hash_type(
          trusted_vault_pb::VaultMetadata_HashType_PBKDF2_AES256_1234);
      break;
    case ::cryptohome::HASH_TYPE_SHA256_TOP_HALF:
      vault_metadata.set_hash_type(
          trusted_vault_pb::VaultMetadata_HashType_SHA256_TOP_HALF);
      break;
    default:
      // Default is necessary because this proto compiles with sentinels such as
      // `INT_MIN_SENTINEL_DO_NOT_USE_`.
      DVLOG(1) << "Unknown hash type: "
               << static_cast<int>(key_store_metadata.hash_type());
      std::move(callback).Run(std::nullopt);
      return;
  }
  vault_metadata.set_hash_salt(key_store_metadata.hash_salt());
  vault_metadata.set_cert_path(key_store_metadata.cert_path());
  vault_metadata.SerializeToString(vault.mutable_vault_metadata());

  vault.set_recovery_key(chosen_key_store->wrapped_recovery_key());
  const cryptohome::WrappedSecurityDomainKey& wrapped_security_domain_key =
      chosen_key_store->wrapped_security_domain_key();
  trusted_vault_pb::ApplicationKey* application_key =
      vault.add_application_keys();
  application_key->set_key_name(wrapped_security_domain_key.key_name());
  application_key->mutable_asymmetric_key_pair()->set_public_key(
      wrapped_security_domain_key.public_key());
  application_key->mutable_asymmetric_key_pair()->set_wrapped_private_key(
      wrapped_security_domain_key.wrapped_private_key());
  application_key->mutable_asymmetric_key_pair()->set_wrapping_key(
      wrapped_security_domain_key.wrapped_wrapping_key());
  vault.mutable_chrome_os_metadata()->set_device_id(device_id_);

  std::move(callback).Run(std::move(vault));
}

}  // namespace trusted_vault
