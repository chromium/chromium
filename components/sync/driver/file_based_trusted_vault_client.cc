// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/file_based_trusted_vault_client.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "components/os_crypt/os_crypt.h"
#include "components/sync/protocol/local_trusted_vault.pb.h"

namespace syncer {

namespace {

constexpr base::TaskTraits kTaskTraits = {
    base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

sync_pb::LocalTrustedVault ReadEncryptedFile(const base::FilePath& file_path) {
  sync_pb::LocalTrustedVault proto;
  std::string ciphertext;
  std::string decrypted_content;
  if (base::ReadFileToString(file_path, &ciphertext) &&
      OSCrypt::DecryptString(ciphertext, &decrypted_content)) {
    proto.ParseFromString(decrypted_content);
  }

  return proto;
}

void WriteToDisk(const sync_pb::LocalTrustedVault& data,
                 const base::FilePath& file_path) {
  std::string encrypted_data;
  if (!OSCrypt::EncryptString(data.SerializeAsString(), &encrypted_data)) {
    DLOG(ERROR) << "Failed to encrypt trusted vault file.";
    return;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(file_path,
                                                      encrypted_data)) {
    DLOG(ERROR) << "Failed to write trusted vault file.";
  }
}

}  // namespace

//
class FileBasedTrustedVaultClient::Backend
    : public base::RefCountedThreadSafe<Backend> {
 public:
  explicit Backend(const base::FilePath& file_path) : file_path_(file_path) {}

  void ReadDataFromDisk() { data_ = ReadEncryptedFile(file_path_); }

  std::vector<std::string> FetchKeys(const std::string& gaia_id) {
    const sync_pb::LocalTrustedVaultPerUser* per_user_vault =
        FindUserVault(gaia_id);

    std::vector<std::string> keys;
    if (per_user_vault) {
      for (const sync_pb::LocalTrustedVaultKey& key : per_user_vault->key()) {
        keys.push_back(key.key_material());
      }
    }

    return keys;
  }

  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::string>& keys) {
    // Find or create user for |gaid_id|.
    sync_pb::LocalTrustedVaultPerUser* per_user_vault = FindUserVault(gaia_id);
    if (!per_user_vault) {
      per_user_vault = data_.add_user();
      per_user_vault->set_gaia_id(gaia_id);
    }

    // Replace all keys.
    per_user_vault->clear_key();
    for (const std::string& key : keys) {
      per_user_vault->add_key()->set_key_material(key);
    }

    WriteToDisk(data_, file_path_);
  }

 private:
  friend class base::RefCountedThreadSafe<Backend>;

  ~Backend() = default;

  // Finds the per-user vault in |data_| for |gaia_id|. Returns null if not
  // found.
  sync_pb::LocalTrustedVaultPerUser* FindUserVault(const std::string& gaia_id) {
    for (int i = 0; i < data_.user_size(); ++i) {
      if (data_.user(i).gaia_id() == gaia_id) {
        return data_.mutable_user(i);
      }
    }
    return nullptr;
  }

  const base::FilePath file_path_;

  sync_pb::LocalTrustedVault data_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

FileBasedTrustedVaultClient::FileBasedTrustedVaultClient(
    const base::FilePath& file_path)
    : file_path_(file_path),
      backend_task_runner_(base::CreateSequencedTaskRunner(kTaskTraits)) {}

FileBasedTrustedVaultClient::~FileBasedTrustedVaultClient() = default;

void FileBasedTrustedVaultClient::FetchKeys(
    const std::string& gaia_id,
    base::OnceCallback<void(const std::vector<std::string>&)> cb) {
  TriggerLazyInitializationIfNeeded();
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&Backend::FetchKeys, backend_, gaia_id), std::move(cb));
}

void FileBasedTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::string>& keys) {
  TriggerLazyInitializationIfNeeded();
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Backend::StoreKeys, backend_, gaia_id, keys));
}

void FileBasedTrustedVaultClient::WaitForFlushForTesting(
    base::OnceClosure cb) const {
  backend_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         std::move(cb));
}

void FileBasedTrustedVaultClient::TriggerLazyInitializationIfNeeded() {
  if (backend_) {
    return;
  }

  backend_ = base::MakeRefCounted<Backend>(file_path_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Backend::ReadDataFromDisk, backend_));
}

bool FileBasedTrustedVaultClient::IsInitializationTriggeredForTesting() const {
  return backend_ != nullptr;
}

}  // namespace syncer
