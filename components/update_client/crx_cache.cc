// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_cache.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "components/prefs/json_pref_store.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace update_client {

// Intended to be used in a base::SequenceBound: functions may block.
class CrxCacheSynchronous {
 public:
  virtual ~CrxCacheSynchronous() = default;
  virtual std::multimap<std::string, std::string> ListHashesByAppId() const = 0;
  virtual base::expected<base::FilePath, UnpackerError> GetByHash(
      const std::string& hash) const = 0;
  virtual base::expected<base::FilePath, UnpackerError> Put(
      const base::FilePath& file,
      const std::string& app_id,
      const std::string& hash) = 0;
  virtual void RemoveAll(const std::string& app_id) = 0;
  virtual void RemoveIfNot(const std::vector<std::string>& app_ids) = 0;
};

// CrxCacheImpl uses a metadata.json file of the following format:
// {
//   "hashes": {
//     "hash1": {"appid": "appid1"},
//     "hash2": {"appid": "appid1"},
//      ...
//   }
// }
// and stores files at:
//   cache_root/hash1
//   cache_root/hash2
//   ...
class CrxCacheImpl : public CrxCacheSynchronous {
 public:
  CrxCacheImpl(const CrxCacheImpl&) = delete;
  CrxCacheImpl& operator=(const CrxCacheImpl&) = delete;

  explicit CrxCacheImpl(const base::FilePath& cache_root);

  // Overrides for CrxCacheSynchronous:
  std::multimap<std::string, std::string> ListHashesByAppId() const override;
  base::expected<base::FilePath, UnpackerError> GetByHash(
      const std::string& hash) const override;
  base::expected<base::FilePath, UnpackerError> Put(
      const base::FilePath& file,
      const std::string& app_id,
      const std::string& hash) override;
  void RemoveAll(const std::string& app_id) override;
  void RemoveIfNot(const std::vector<std::string>& app_ids) override;

 private:
  void Remove(const std::string& hash);

  SEQUENCE_CHECKER(sequence_checker_);
  const base::FilePath cache_root_;

  // Note: `~JsonPrefStore` calls `JsonPrefStore::CommitPendingWrite()`.
  scoped_refptr<JsonPrefStore> metadata_;
};

CrxCacheImpl::CrxCacheImpl(const base::FilePath& cache_root)
    : cache_root_(cache_root),
      metadata_(base::MakeRefCounted<JsonPrefStore>(
          cache_root_.Append(FILE_PATH_LITERAL("metadata.json")))) {
  base::CreateDirectory(cache_root_);
  metadata_->ReadPrefs();
  absl::flat_hash_set<std::string> expected_basenames({"metadata.json"});
  absl::flat_hash_set<std::string> found_basenames;
  const base::Value* hashes_key = nullptr;
  if (!metadata_->GetValue("hashes", &hashes_key) || !hashes_key->is_dict()) {
    metadata_->SetValue("hashes", base::Value(base::DictValue()), 0);
    CHECK(metadata_->GetValue("hashes", &hashes_key) && hashes_key->is_dict());
  }
  for (const auto [hash, value] : hashes_key->GetDict()) {
    expected_basenames.insert(hash);
  }

  // Remove files that are missing metadata entries.
  base::FileEnumerator(cache_root_, false, base::FileEnumerator::FILES)
      .ForEach([&expected_basenames,
                &found_basenames](const base::FilePath& file_path) {
        if (!expected_basenames.contains(file_path.BaseName().AsUTF8Unsafe())) {
          RetryFileOperation(&base::DeleteFile, file_path);
        } else {
          found_basenames.insert(file_path.BaseName().AsUTF8Unsafe());
        }
      });

  // Remove metadata entries that are missing files.
  for (const auto& hash : expected_basenames) {
    if (!found_basenames.contains(hash)) {
      Remove(hash);
    }
  }
}


std::multimap<std::string, std::string> CrxCacheImpl::ListHashesByAppId()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::multimap<std::string, std::string> hashes;
  const base::Value* hashes_key = nullptr;
  if (!metadata_->GetValue("hashes", &hashes_key) || !hashes_key->is_dict()) {
    return hashes;
  }
  for (const auto [hash, value] : hashes_key->GetDict()) {
    if (value.is_dict()) {
      const std::string* item_appid = value.GetDict().FindString("appid");
      if (item_appid) {
        hashes.insert({*item_appid, hash});
      }
    }
  }
  return hashes;
}

base::expected<base::FilePath, UnpackerError> CrxCacheImpl::GetByHash(
    const std::string& hash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* hashes_key = nullptr;
  if (!metadata_->GetValue("hashes", &hashes_key) || !hashes_key->is_dict()) {
    return base::unexpected(UnpackerError::kCrxCacheMetadataCorrupted);
  }
  if (!hashes_key->GetDict().contains(hash)) {
    return base::unexpected(UnpackerError::kCrxCacheFileNotCached);
  }
  return cache_root_.AppendUTF8(hash);
}

base::expected<base::FilePath, UnpackerError> CrxCacheImpl::Put(
    const base::FilePath& file,
    const std::string& app_id,
    const std::string& hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath dest = cache_root_.AppendUTF8(hash);
  if (file == dest) {
    return dest;  // Already cached.
  }

  RemoveAll(app_id);
  if (!base::CreateDirectory(cache_root_)) {
    return base::unexpected(UnpackerError::kFailedToCreateCacheDir);
  }
  if (!base::Move(file, dest)) {
    return base::unexpected(UnpackerError::kFailedToAddToCache);
  }
  LOG_IF(ERROR, !base::IsDirectoryEmpty(file.DirName()))
      << "Unexpected, directory not empty: " << file.DirName();
  if (!DeleteEmptyDirectory(file.DirName())) {
    PLOG(ERROR) << "Error deleting directory: " << file.DirName();
  }

  // Update metadata.
  base::Value::Dict data;
  data.Set("appid", app_id);
  metadata_->SetValue(base::StrCat({"hashes.", hash}),
                      base::Value(std::move(data)), 0);
  return dest;
}

void CrxCacheImpl::RemoveAll(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  absl::flat_hash_set<std::string> eviction_hashes;
  const base::Value* hashes_key = nullptr;
  if (!metadata_->GetValue("hashes", &hashes_key) || !hashes_key->is_dict()) {
    return;
  }
  for (const auto [hash, value] : hashes_key->GetDict()) {
    if (value.is_dict()) {
      const std::string* item_app_id = value.GetDict().FindString("appid");
      if (item_app_id && app_id == *item_app_id) {
        eviction_hashes.insert(hash);
      }
    }
  }
  for (const auto& hash : eviction_hashes) {
    Remove(hash);
  }
}

void CrxCacheImpl::RemoveIfNot(const std::vector<std::string>& app_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  absl::flat_hash_set<std::string> retained_ids(app_ids.begin(), app_ids.end());
  for (const auto& [id, hash] : ListHashesByAppId()) {
    if (!retained_ids.contains(id)) {
      RemoveAll(id);
    }
  }
}

void CrxCacheImpl::Remove(const std::string& hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RetryFileOperation(&base::DeleteFile, cache_root_.AppendUTF8(hash));
  metadata_->RemoveValue(base::StrCat({"hashes.", hash}), 0);
}

// CrxCacheError implements CrxCache but always returns an error.
class CrxCacheError : public CrxCacheSynchronous {
 public:
  CrxCacheError(const CrxCacheError&) = delete;
  CrxCacheError& operator=(const CrxCacheError&) = delete;

  CrxCacheError() = default;

  // Overrides for CrxCache:
  std::multimap<std::string, std::string> ListHashesByAppId() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return {};
  }
  base::expected<base::FilePath, UnpackerError> GetByHash(
      const std::string& hash) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::unexpected(UnpackerError::kCrxCacheNotProvided);
  }
  base::expected<base::FilePath, UnpackerError> Put(
      const base::FilePath& file,
      const std::string& app_id,
      const std::string& hash) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::unexpected(UnpackerError::kCrxCacheNotProvided);
  }
  void RemoveAll(const std::string& app_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  void RemoveIfNot(const std::vector<std::string>& app_ids) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

CrxCache::CrxCache(std::optional<base::FilePath> path) {
  if (path) {
    delegate_ = base::SequenceBound<CrxCacheImpl>(
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}), *path);
  } else {
    delegate_ = base::SequenceBound<CrxCacheError>(
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  }
}

CrxCache::~CrxCache() = default;

void CrxCache::ListHashesByAppId(
    base::OnceCallback<void(const std::multimap<std::string, std::string>&)>
        callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_.AsyncCall(&CrxCacheSynchronous::ListHashesByAppId)
      .Then(std::move(callback));
}

void CrxCache::GetByHash(
    const std::string& hash,
    base::OnceCallback<void(base::expected<base::FilePath, UnpackerError>)>
        callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_.AsyncCall(&CrxCacheSynchronous::GetByHash)
      .WithArgs(hash)
      .Then(std::move(callback));
}

void CrxCache::Put(
    const base::FilePath& file,
    const std::string& app_id,
    const std::string& hash,
    base::OnceCallback<void(base::expected<base::FilePath, UnpackerError>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_.AsyncCall(&CrxCacheSynchronous::Put)
      .WithArgs(file, app_id, hash)
      .Then(std::move(callback));
}

void CrxCache::RemoveAll(const std::string& app_id,
                         base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_.AsyncCall(&CrxCacheSynchronous::RemoveAll)
      .WithArgs(app_id)
      .Then(std::move(callback));
}

void CrxCache::RemoveIfNot(const std::vector<std::string>& app_ids,
                           base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_.AsyncCall(&CrxCacheSynchronous::RemoveIfNot)
      .WithArgs(app_ids)
      .Then(std::move(callback));
}

}  // namespace update_client
