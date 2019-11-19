// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_content_database.h"

#include <unordered_set>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feed/core/feed_content_mutation.h"
#include "components/feed/core/feed_content_operation.h"
#include "components/feed/core/proto/content_storage.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace feed {

namespace {

const char kContentDatabaseFolder[] = "content";

// Content writes vary a lot in size, loading full page will will have a couple
// dozen writes totaling a couple dozen KB, but there's a lot of variability.
// This should result in some batching while also keeping the memory impact very
// small.
const size_t kDatabaseWriteBufferSizeBytes = 8 * 1024;                 // 8KB
const size_t kDatabaseWriteBufferSizeBytesForLowEndDevice = 4 * 1024;  // 4KB

leveldb::ReadOptions CreateReadOptions() {
  leveldb::ReadOptions opts;
  opts.fill_cache = false;
  return opts;
}

bool DatabaseKeyFilter(const std::unordered_set<std::string>& key_set,
                       const std::string& key) {
  return key_set.find(key) != key_set.end();
}

bool DatabasePrefixFilter(const std::string& key_prefix,
                          const std::string& key) {
  return base::StartsWith(key, key_prefix, base::CompareCase::SENSITIVE);
}

}  // namespace

FeedContentDatabase::FeedContentDatabase(
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& database_folder)
    : database_status_(InitStatus::kNotInitialized),
      task_runner_(
          base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                           base::TaskPriority::USER_VISIBLE})),
      storage_database_(proto_database_provider->GetDB<ContentStorageProto>(
          leveldb_proto::ProtoDbType::FEED_CONTENT_DATABASE,
          database_folder.AppendASCII(kContentDatabaseFolder),
          task_runner_)) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&FeedContentDatabase::InitInternal,
                                        weak_ptr_factory_.GetWeakPtr()));
}

// Used for testing.
FeedContentDatabase::FeedContentDatabase(
    std::unique_ptr<leveldb_proto::ProtoDatabase<ContentStorageProto>>
        storage_database,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : database_status_(InitStatus::kNotInitialized),
      task_runner_(task_runner),
      storage_database_(std::move(storage_database)) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&FeedContentDatabase::InitInternal,
                                        weak_ptr_factory_.GetWeakPtr()));
}

FeedContentDatabase::~FeedContentDatabase() = default;

bool FeedContentDatabase::IsInitialized() const {
  return database_status_ == InitStatus::kOK;
}

void FeedContentDatabase::InitInternal() {
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  options.write_buffer_size = base::SysInfo::IsLowEndDevice()
                                  ? kDatabaseWriteBufferSizeBytesForLowEndDevice
                                  : kDatabaseWriteBufferSizeBytes;

  storage_database_->Init(
      options, base::BindOnce(&FeedContentDatabase::OnDatabaseInitialized,
                              weak_ptr_factory_.GetWeakPtr()));
}

void FeedContentDatabase::LoadContent(const std::vector<std::string>& keys,
                                      ContentLoadCallback callback) {
  std::unordered_set<std::string> key_set(keys.begin(), keys.end());

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FeedContentDatabase::LoadEntriesWithFilterInternal,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindRepeating(&DatabaseKeyFilter, std::move(key_set)),
          std::move(callback)));
}

void FeedContentDatabase::LoadContentByPrefix(const std::string& prefix,
                                              ContentLoadCallback callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FeedContentDatabase::LoadEntriesWithFilterInternal,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindRepeating(&DatabasePrefixFilter, std::move(prefix)),
          std::move(callback)));
}

void FeedContentDatabase::LoadEntriesWithFilterInternal(
    const leveldb_proto::KeyFilter& key_filter,
    ContentLoadCallback callback) {
  storage_database_->LoadEntriesWithFilter(
      std::move(key_filter), CreateReadOptions(), /* target_prefix */ "",
      base::BindOnce(&FeedContentDatabase::OnLoadEntriesForLoadContent,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(callback)));
}

void FeedContentDatabase::LoadAllContentKeys(ContentKeyCallback callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FeedContentDatabase::LoadKeysInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedContentDatabase::LoadKeysInternal(ContentKeyCallback callback) {
  storage_database_->LoadKeys(
      base::BindOnce(&FeedContentDatabase::OnLoadKeysForLoadAllContentKeys,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(callback)));
}

void FeedContentDatabase::CommitContentMutation(
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK(content_mutation);

  UMA_HISTOGRAM_COUNTS_100(
      "ContentSuggestions.Feed.ContentStorage.CommitMutationCount",
      content_mutation->Size());

  if (content_mutation->Empty()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), true));
    return;
  }

  PerformNextOperation(std::move(content_mutation), std::move(callback));
}

void FeedContentDatabase::PerformNextOperation(
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK(!content_mutation->Empty());

  ContentOperation operation = content_mutation->TakeFirstOperation();

  switch (operation.type()) {
    case ContentOperation::CONTENT_DELETE:
      // TODO(gangwu): If deletes are continuous, we should combine them into
      // one commit.
      DeleteContent(std::move(operation), std::move(content_mutation),
                    std::move(callback));
      break;
    case ContentOperation::CONTENT_DELETE_BY_PREFIX:
      DeleteContentByPrefix(std::move(operation), std::move(content_mutation),
                            std::move(callback));
      break;
    case ContentOperation::CONTENT_UPSERT:
      // TODO(gangwu): If upserts are continuous, we should combine them into
      // one commit.
      UpsertContent(std::move(operation), std::move(content_mutation),
                    std::move(callback));
      break;
    case ContentOperation::CONTENT_DELETE_ALL:
      DeleteAllContent(std::move(operation), std::move(content_mutation),
                       std::move(callback));
      break;
    default:
      // Operation type is not supported, therefore failing immediately.
      task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(std::move(callback), false));
  }
}

void FeedContentDatabase::UpsertContent(
    ContentOperation operation,
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK_EQ(operation.type(), ContentOperation::CONTENT_UPSERT);

  auto contents_to_save = std::make_unique<StorageEntryVector>();
  ContentStorageProto proto;
  proto.set_key(operation.key());
  proto.set_content_data(operation.value());
  contents_to_save->emplace_back(proto.key(), std::move(proto));

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FeedContentDatabase::UpdateEntriesInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(contents_to_save),
                     std::make_unique<std::vector<std::string>>(),
                     std::move(content_mutation), std::move(callback)));
}

void FeedContentDatabase::DeleteContent(
    ContentOperation operation,
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK_EQ(operation.type(), ContentOperation::CONTENT_DELETE);

  auto content_to_delete = std::make_unique<std::vector<std::string>>(
      std::initializer_list<std::string>({operation.key()}));

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FeedContentDatabase::UpdateEntriesInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::make_unique<StorageEntryVector>(),
                     std::move(content_to_delete), std::move(content_mutation),
                     std::move(callback)));
}

void FeedContentDatabase::UpdateEntriesInternal(
    std::unique_ptr<StorageEntryVector> entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  storage_database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::BindOnce(&FeedContentDatabase::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(content_mutation), std::move(callback)));
}

void FeedContentDatabase::DeleteContentByPrefix(
    ContentOperation operation,
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK_EQ(operation.type(), ContentOperation::CONTENT_DELETE_BY_PREFIX);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FeedContentDatabase::UpdateEntriesWithRemoveFilterInternal,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindRepeating(&DatabasePrefixFilter, operation.prefix()),
          std::move(content_mutation), std::move(callback)));
}

void FeedContentDatabase::DeleteAllContent(
    ContentOperation operation,
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK_EQ(operation.type(), ContentOperation::CONTENT_DELETE_ALL);

  std::string key_prefix = "";
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FeedContentDatabase::UpdateEntriesWithRemoveFilterInternal,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindRepeating(&DatabasePrefixFilter, std::move(key_prefix)),
          std::move(content_mutation), std::move(callback)));
}

void FeedContentDatabase::UpdateEntriesWithRemoveFilterInternal(
    const leveldb_proto::KeyFilter& key_filter,
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  storage_database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<StorageEntryVector>(), std::move(key_filter),
      base::BindOnce(&FeedContentDatabase::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(content_mutation), std::move(callback)));
}

void FeedContentDatabase::OnDatabaseInitialized(InitStatus status) {
  DCHECK_EQ(database_status_, InitStatus::kNotInitialized);
  database_status_ = status;
}

void FeedContentDatabase::OnLoadEntriesForLoadContent(
    base::TimeTicks start_time,
    ContentLoadCallback callback,
    bool success,
    std::unique_ptr<std::vector<ContentStorageProto>> content) {
  base::TimeDelta load_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("ContentSuggestions.Feed.ContentStorage.LoadTime",
                      load_time);

  std::vector<KeyAndData> results;
  if (success) {
    for (const auto& proto : *content) {
      DCHECK(proto.has_key());
      DCHECK(proto.has_content_data());

      results.emplace_back(proto.key(), proto.content_data());
    }
  }

  std::move(callback).Run(success, std::move(results));
}

void FeedContentDatabase::OnLoadKeysForLoadAllContentKeys(
    base::TimeTicks start_time,
    ContentKeyCallback callback,
    bool success,
    std::unique_ptr<std::vector<std::string>> keys) {
  if (success) {
    // Typical usage has a max around 300(100 cards, 3 pieces of content per
    // card), could grow forever through heavy usage of dismiss. If typically
    // usage changes, 1000 maybe too small.
    UMA_HISTOGRAM_COUNTS_1000("ContentSuggestions.Feed.ContentStorage.Count",
                              keys->size());
  }

  base::TimeDelta load_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("ContentSuggestions.Feed.ContentStorage.LoadKeysTime",
                      load_time);

  // We std::move the |*keys|'s entries to |callback|, after that, |keys| become
  // a pointer holding an empty vector, then we can safely delete unique_ptr
  // |keys| when it out of scope.
  std::move(callback).Run(success, std::move(*keys));
}

void FeedContentDatabase::OnOperationCommitted(
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback,
    bool success) {
  // Commit is unsuccessful, skip processing the other operations since
  // ContentStorage.java requires "In the event of a failure, processing is
  // halted immediately".
  if (!success) {
    std::move(callback).Run(success);
    return;
  }

  // All operations were committed successfully, call |callback|.
  if (content_mutation->Empty()) {
    base::TimeDelta commit_time =
        base::TimeTicks::Now() - content_mutation->GetStartTime();
    UMA_HISTOGRAM_TIMES(
        "ContentSuggestions.Feed.ContentStorage.OperationCommitTime",
        commit_time);

    std::move(callback).Run(success);
    return;
  }

  PerformNextOperation(std::move(content_mutation), std::move(callback));
}

}  // namespace feed
