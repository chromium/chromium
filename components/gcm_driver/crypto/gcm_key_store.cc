// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_key_store.h"

#include <stddef.h>

#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "crypto/random.h"

namespace gcm {

namespace {

using EntryVectorType =
    leveldb_proto::ProtoDatabase<EncryptionData>::KeyEntryVector;

// Number of cryptographically secure random bytes to generate as a key pair's
// authentication secret. Must be at least 16 bytes.
const size_t kAuthSecretBytes = 16;

std::string DatabaseKey(const std::string& app_id,
                        const std::string& authorized_entity) {
  DCHECK_EQ(std::string::npos, app_id.find(','));
  DCHECK_EQ(std::string::npos, authorized_entity.find(','));
  DCHECK_NE("*", authorized_entity) << "Wildcards require special handling";
  return authorized_entity.empty()
             ? app_id  // No comma, for compatibility with existing keys.
             : app_id + ',' + authorized_entity;
}

}  // namespace

enum class GCMKeyStore::State {
   UNINITIALIZED,
   INITIALIZING,
   INITIALIZED,
   FAILED
};

GCMKeyStore::GCMKeyStore(
    const base::FilePath& key_store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : key_store_path_(key_store_path),
      blocking_task_runner_(blocking_task_runner),
      state_(State::UNINITIALIZED) {
  DCHECK(blocking_task_runner);
}

GCMKeyStore::~GCMKeyStore() = default;

void GCMKeyStore::GetKeys(const std::string& app_id,
                          const std::string& authorized_entity,
                          bool fallback_to_empty_authorized_entity,
                          KeysCallback callback) {
  LazyInitialize(
      base::BindOnce(&GCMKeyStore::GetKeysAfterInitialize,
                     weak_factory_.GetWeakPtr(), app_id, authorized_entity,
                     fallback_to_empty_authorized_entity, std::move(callback)));
}

void GCMKeyStore::GetKeysAfterInitialize(
    const std::string& app_id,
    const std::string& authorized_entity,
    bool fallback_to_empty_authorized_entity,
    KeysCallback callback) {
  DCHECK(state_ == State::INITIALIZED || state_ == State::FAILED);
  bool success = false;

  if (state_ == State::INITIALIZED) {
    auto outer_iter = key_data_.find(app_id);
    if (outer_iter != key_data_.end()) {
      const auto& inner_map = outer_iter->second;
      auto inner_iter = inner_map.find(authorized_entity);
      if (fallback_to_empty_authorized_entity && inner_iter == inner_map.end())
        inner_iter = inner_map.find(std::string());
      if (inner_iter != inner_map.end()) {
        const auto& map_entry = inner_iter->second;
        std::move(callback).Run(map_entry.first->Copy(), map_entry.second);
        success = true;
      }
    }
  }

  if (!success)
    std::move(callback).Run(nullptr /* key */, std::string() /* auth_secret */);
}

void GCMKeyStore::CreateKeys(const std::string& app_id,
                             const std::string& authorized_entity,
                             KeysCallback callback) {
  LazyInitialize(base::BindOnce(&GCMKeyStore::CreateKeysAfterInitialize,
                                weak_factory_.GetWeakPtr(), app_id,
                                authorized_entity, std::move(callback)));
}

void GCMKeyStore::CreateKeysAfterInitialize(
    const std::string& app_id,
    const std::string& authorized_entity,
    KeysCallback callback) {
  DCHECK(state_ == State::INITIALIZED || state_ == State::FAILED);
  if (state_ != State::INITIALIZED) {
    std::move(callback).Run(nullptr /* key */, std::string() /* auth_secret */);
    return;
  }

  // Only allow creating new keys if no keys currently exist. Multiple Instance
  // ID tokens can share an app_id (with different authorized entities), but
  // InstanceID tokens can't share an app_id with a non-InstanceID registration.
  // This invariant is necessary for the fallback_to_empty_authorized_entity
  // mode of GetKey (needed by GCMEncryptionProvider::DecryptMessage, which
  // can't distinguish Instance ID tokens from non-InstanceID registrations).
  DCHECK(!key_data_.count(app_id) ||
         (!authorized_entity.empty() &&
          !key_data_[app_id].count(authorized_entity) &&
          !key_data_[app_id].count(std::string())))
      << "Instance ID tokens cannot share an app_id with a non-InstanceID GCM "
         "registration";

  std::unique_ptr<crypto::ECPrivateKey> key(crypto::ECPrivateKey::Create());

  if (!key) {
    NOTREACHED_IN_MIGRATION() << "Unable to initialize a P-256 key pair.";

    std::move(callback).Run(nullptr /* key */, std::string() /* auth_secret */);
    return;
  }

  // Create the authentication secret, which has to be a cryptographically
  // secure random number of at least 128 bits (16 bytes).
  std::string auth_secret(kAuthSecretBytes, '\0');
  crypto::RandBytes(base::as_writable_byte_span(auth_secret));

  // Store the keys in a new EncryptionData object.
  EncryptionData encryption_data;
  encryption_data.set_app_id(app_id);
  if (!authorized_entity.empty())
    encryption_data.set_authorized_entity(authorized_entity);
  encryption_data.set_auth_secret(auth_secret);

  std::string private_key;
  bool success = GetRawPrivateKey(*key, &private_key);
  DCHECK(success);
  encryption_data.set_private_key(private_key);

  // Write them immediately to our cache, so subsequent calls to
  // {Get/Create/Remove}Keys can see them.
  key_data_[app_id][authorized_entity] = {key->Copy(), auth_secret};

  std::unique_ptr<EntryVectorType> entries_to_save(new EntryVectorType());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  entries_to_save->push_back(
      std::make_pair(DatabaseKey(app_id, authorized_entity), encryption_data));

  database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::BindOnce(&GCMKeyStore::DidStoreKeys, weak_factory_.GetWeakPtr(),
                     std::move(key), auth_secret, std::move(callback)));
}

void GCMKeyStore::DidStoreKeys(std::unique_ptr<crypto::ECPrivateKey> pair,
                               const std::string& auth_secret,
                               KeysCallback callback,
                               bool success) {
  if (!success) {
    DVLOG(1) << "Unable to store the created key in the GCM Key Store.";

    // Our cache is now inconsistent. Reject requests until restarted.
    state_ = State::FAILED;

    std::move(callback).Run(nullptr /* key */, std::string() /* auth_secret */);
    return;
  }

  std::move(callback).Run(std::move(pair), auth_secret);
}

void GCMKeyStore::RemoveKeys(const std::string& app_id,
                             const std::string& authorized_entity,
                             base::OnceClosure callback) {
  LazyInitialize(base::BindOnce(&GCMKeyStore::RemoveKeysAfterInitialize,
                                weak_factory_.GetWeakPtr(), app_id,
                                authorized_entity, std::move(callback)));
}

void GCMKeyStore::RemoveKeysAfterInitialize(
    const std::string& app_id,
    const std::string& authorized_entity,
    base::OnceClosure callback) {
  DCHECK(state_ == State::INITIALIZED || state_ == State::FAILED);

  const auto& outer_iter = key_data_.find(app_id);
  if (outer_iter == key_data_.end() || state_ != State::INITIALIZED) {
    std::move(callback).Run();
    return;
  }

  std::unique_ptr<EntryVectorType> entries_to_save(new EntryVectorType());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  bool had_keys = false;
  auto& inner_map = outer_iter->second;
  for (auto it = inner_map.begin(); it != inner_map.end();) {
    // Wildcard "*" matches all non-empty authorized entities (InstanceID only).
    if (authorized_entity == "*" ? !it->first.empty()
                                 : it->first == authorized_entity) {
      had_keys = true;

      keys_to_remove->push_back(DatabaseKey(app_id, it->first));

      // Clear keys immediately from our cache, so subsequent calls to
      // {Get/Create/Remove}Keys don't see them.
      it = inner_map.erase(it);
    } else {
      ++it;
    }
  }
  if (!had_keys) {
    std::move(callback).Run();
    return;
  }
  if (inner_map.empty())
    key_data_.erase(app_id);

  database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::BindOnce(&GCMKeyStore::DidRemoveKeys, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void GCMKeyStore::DidRemoveKeys(base::OnceClosure callback, bool success) {
  if (!success) {
    DVLOG(1) << "Unable to delete a key from the GCM Key Store.";

    // Our cache is now inconsistent. Reject requests until restarted.
    state_ = State::FAILED;
  }

  std::move(callback).Run();
}

void GCMKeyStore::DidUpgradeDatabase(bool success) {
  if (!success) {
    DVLOG(1) << "Unable to upgrade the GCM Key Store database.";
    // Our cache is now inconsistent. Reject requests until restarted.
    state_ = State::FAILED;
    delayed_task_controller_.SetReady();
    return;
  }

  database_->LoadEntries(
      base::BindOnce(&GCMKeyStore::DidLoadKeys, weak_factory_.GetWeakPtr()));
}

void GCMKeyStore::LazyInitialize(base::OnceClosure done_closure) {
  if (delayed_task_controller_.CanRunTaskWithoutDelay()) {
    std::move(done_closure).Run();
    return;
  }

  delayed_task_controller_.AddTask(std::move(done_closure));
  if (state_ == State::INITIALIZING)
    return;

  state_ = State::INITIALIZING;

  database_ = leveldb_proto::ProtoDatabaseProvider::GetUniqueDB<EncryptionData>(
      leveldb_proto::ProtoDbType::GCM_KEY_STORE, key_store_path_,
      blocking_task_runner_);

  database_->Init(
      leveldb_proto::CreateSimpleOptions(),
      base::BindOnce(&GCMKeyStore::DidInitialize, weak_factory_.GetWeakPtr()));
}

void GCMKeyStore::DidInitialize(leveldb_proto::Enums::InitStatus status) {
  bool success = status == leveldb_proto::Enums::kOK;
  if (!success) {
    DVLOG(1) << "Unable to initialize the GCM Key Store.";
    state_ = State::FAILED;

    delayed_task_controller_.SetReady();
    return;
  }

  database_->LoadEntries(
      base::BindOnce(&GCMKeyStore::DidLoadKeys, weak_factory_.GetWeakPtr()));
}

void GCMKeyStore::UpgradeDatabase(
    std::unique_ptr<std::vector<EncryptionData>> entries) {
  std::unique_ptr<EntryVectorType> entries_to_save =
      std::make_unique<EntryVectorType>();
  std::unique_ptr<std::vector<std::string>> keys_to_remove =
      std::make_unique<std::vector<std::string>>();

  // Loop over entries, create list of database entries to overwrite.
  for (EncryptionData& entry : *entries) {
    if (!entry.keys_size())
      continue;
    std::string decrypted_private_key;
    if (!DecryptPrivateKey(entry.keys(0).private_key(),
                           &decrypted_private_key)) {
      DVLOG(1) << "Unable to decrypt private key: "
               << entry.keys(0).private_key();
      state_ = State::FAILED;
      delayed_task_controller_.SetReady();
      return;
    }

    entry.set_private_key(decrypted_private_key);
    entry.clear_keys();
    entries_to_save->push_back(std::make_pair(
        DatabaseKey(entry.app_id(), entry.authorized_entity()), entry));
  }

  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove),
                           base::BindOnce(&GCMKeyStore::DidUpgradeDatabase,
                                          weak_factory_.GetWeakPtr()));
}

void GCMKeyStore::DidLoadKeys(
    bool success,
    std::unique_ptr<std::vector<EncryptionData>> entries) {
  if (!success) {
    DVLOG(1) << "Unable to load entries into the GCM Key Store.";
    state_ = State::FAILED;

    delayed_task_controller_.SetReady();
    return;
  }

  for (const EncryptionData& entry : *entries) {
    std::string authorized_entity;
    if (entry.has_authorized_entity())
      authorized_entity = entry.authorized_entity();
    std::unique_ptr<crypto::ECPrivateKey> key;

    // The old format of EncryptionData has a KeyPair in it. Previously
    // we used to cache the key pair and auth secret in key_data_.
    // The new code adds the pair {ECPrivateKey, auth_secret} to
    // key_data_ instead.
    if (entry.keys_size()) {
      if (state_ == State::FAILED)
        return;

      // Old format of EncryptionData. Upgrade database so there are no such
      // entries. We'll reload keys from the database once this is done.
      UpgradeDatabase(std::move(entries));
      return;
    } else {
      std::string private_key_str = entry.private_key();
      if (private_key_str.empty())
        continue;
      std::vector<uint8_t> private_key(private_key_str.begin(),
                                       private_key_str.end());
      key = crypto::ECPrivateKey::CreateFromPrivateKeyInfo(private_key);
    }

    key_data_[entry.app_id()][authorized_entity] =
        std::make_pair(std::move(key), entry.auth_secret());
  }

  state_ = State::INITIALIZED;

  delayed_task_controller_.SetReady();
}

bool GCMKeyStore::DecryptPrivateKey(const std::string& to_decrypt,
                                    std::string* decrypted) {
  DCHECK(decrypted);
  std::vector<uint8_t> to_decrypt_vector(to_decrypt.begin(), to_decrypt.end());
  std::unique_ptr<crypto::ECPrivateKey> key_to_decrypt =
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          to_decrypt_vector);
  if (!key_to_decrypt)
    return false;
  std::vector<uint8_t> decrypted_vector;
  if (!key_to_decrypt->ExportPrivateKey(&decrypted_vector))
    return false;
  decrypted->assign(decrypted_vector.begin(), decrypted_vector.end());
  return true;
}

}  // namespace gcm
