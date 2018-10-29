// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_content_database.h"

#include <unordered_set>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feed/core/feed_content_mutation.h"
#include "components/feed/core/feed_content_operation.h"
#include "components/feed/core/proto/content_storage.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace feed {

namespace {

using StorageEntryVector =
    leveldb_proto::ProtoDatabase<ContentStorageProto>::KeyEntryVector;

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.FeedContentDatabase. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kContentDatabaseUMAClientName[] = "FeedContentDatabase";

const char kContentDatabaseFolder[] = "content";

const size_t kDatabaseWriteBufferSizeBytes = 512 * 1024;
const size_t kDatabaseWriteBufferSizeBytesForLowEndDevice = 128 * 1024;

bool DatabaseKeyFilter(const std::unordered_set<std::string>& key_set,
                       const std::string& key) {
  return key_set.find(key) != key_set.end();
}

bool DatabasePrefixFilter(const std::string& key_prefix,
                          const std::string& key) {
  return base::StartsWith(key, key_prefix, base::CompareCase::SENSITIVE);
}

void ReportOperationResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN(
      "ContentSuggestions.Feed.ContentStorage.OperationCommitSuccess", success);
}

}  // namespace

FeedContentDatabase::FeedContentDatabase(const base::FilePath& database_folder)
    : FeedContentDatabase(
          database_folder,
          std::make_unique<
              leveldb_proto::ProtoDatabaseImpl<ContentStorageProto>>(
              base::CreateSequencedTaskRunnerWithTraits(
                  {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                   base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}))) {}

FeedContentDatabase::FeedContentDatabase(
    const base::FilePath& database_folder,
    std::unique_ptr<leveldb_proto::ProtoDatabase<ContentStorageProto>>
        storage_database)
    : database_status_(UNINITIALIZED),
      storage_database_(std::move(storage_database)),
      weak_ptr_factory_(this) {
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  if (base::SysInfo::IsLowEndDevice()) {
    options.write_buffer_size = kDatabaseWriteBufferSizeBytesForLowEndDevice;
  } else {
    options.write_buffer_size = kDatabaseWriteBufferSizeBytes;
  }

  base::FilePath storage_folder =
      database_folder.AppendASCII(kContentDatabaseFolder);
  storage_database_->Init(
      kContentDatabaseUMAClientName, storage_folder, options,
      base::BindOnce(&FeedContentDatabase::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

FeedContentDatabase::~FeedContentDatabase() = default;

bool FeedContentDatabase::IsInitialized() const {
  return INITIALIZED == database_status_;
}

void FeedContentDatabase::LoadContent(const std::vector<std::string>& keys,
                                      ContentLoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unordered_set<std::string> key_set(keys.begin(), keys.end());

  storage_database_->LoadEntriesWithFilter(
      base::BindRepeating(&DatabaseKeyFilter, std::move(key_set)),
      base::BindOnce(&FeedContentDatabase::OnLoadEntriesForLoadContent,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(callback)));
}

void FeedContentDatabase::LoadContentByPrefix(const std::string& prefix,
                                              ContentLoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_database_->LoadEntriesWithFilter(
      base::BindRepeating(&DatabasePrefixFilter, std::move(prefix)),
      base::BindOnce(&FeedContentDatabase::OnLoadEntriesForLoadContent,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(callback)));
}

void FeedContentDatabase::LoadAllContentKeys(ContentKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_database_->LoadKeys(
      base::BindOnce(&FeedContentDatabase::OnLoadKeysForLoadAllContentKeys,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(callback)));
}

void FeedContentDatabase::CommitContentMutation(
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(content_mutation);

  UMA_HISTOGRAM_COUNTS_100(
      "ContentSuggestions.Feed.ContentStorage.CommitMutationCount",
      content_mutation->Size());

  if (content_mutation->Empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }

  PerformNextOperation(std::move(content_mutation), std::move(callback));
}

void FeedContentDatabase::PerformNextOperation(
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK(!content_mutation->Empty());

  ContentOperation operation = content_mutation->TakeFristOperation();

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
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), false));
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

  storage_database_->UpdateEntries(
      std::move(contents_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&FeedContentDatabase::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(content_mutation), std::move(callback)));
}

void FeedContentDatabase::DeleteContent(
    ContentOperation operation,
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK_EQ(operation.type(), ContentOperation::CONTENT_DELETE);

  auto content_to_delete = std::make_unique<std::vector<std::string>>(
      std::initializer_list<std::string>({operation.key()}));

  storage_database_->UpdateEntries(
      std::make_unique<StorageEntryVector>(), std::move(content_to_delete),
      base::BindOnce(&FeedContentDatabase::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(content_mutation), std::move(callback)));
}

void FeedContentDatabase::DeleteContentByPrefix(
    ContentOperation operation,
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK_EQ(operation.type(), ContentOperation::CONTENT_DELETE_BY_PREFIX);

  storage_database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<StorageEntryVector>(),
      base::BindRepeating(&DatabasePrefixFilter, operation.prefix()),
      base::BindOnce(&FeedContentDatabase::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(content_mutation), std::move(callback)));
}

void FeedContentDatabase::DeleteAllContent(
    ContentOperation operation,
    std::unique_ptr<ContentMutation> content_mutation,
    ConfirmationCallback callback) {
  DCHECK_EQ(operation.type(), ContentOperation::CONTENT_DELETE_ALL);

  std::string key_prefix = "";
  storage_database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<StorageEntryVector>(),
      base::BindRepeating(&DatabasePrefixFilter, std::move(key_prefix)),
      base::BindOnce(&FeedContentDatabase::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(content_mutation), std::move(callback)));
}

void FeedContentDatabase::OnDatabaseInitialized(bool success) {
  DCHECK_EQ(database_status_, UNINITIALIZED);

  if (success) {
    database_status_ = INITIALIZED;
  } else {
    database_status_ = INIT_FAILURE;
    DVLOG(1) << "FeedContentDatabase init failed.";
  }
  UMA_HISTOGRAM_BOOLEAN("ContentSuggestions.Feed.ContentStorage.InitialSuccess",
                        success);
}

void FeedContentDatabase::OnLoadEntriesForLoadContent(
    base::TimeTicks start_time,
    ContentLoadCallback callback,
    bool success,
    std::unique_ptr<std::vector<ContentStorageProto>> content) {
  DVLOG_IF(1, !success) << "FeedContentDatabase load content failed.";
  UMA_HISTOGRAM_BOOLEAN("ContentSuggestions.Feed.ContentStorage.LoadSuccess",
                        success);

  std::vector<KeyAndData> results;
  for (const auto& proto : *content) {
    DCHECK(proto.has_key());
    DCHECK(proto.has_content_data());

    results.emplace_back(proto.key(), proto.content_data());
  }

  base::TimeDelta load_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("ContentSuggestions.Feed.ContentStorage.LoadTime",
                      load_time);

  std::move(callback).Run(success, std::move(results));
}

void FeedContentDatabase::OnLoadKeysForLoadAllContentKeys(
    base::TimeTicks start_time,
    ContentKeyCallback callback,
    bool success,
    std::unique_ptr<std::vector<std::string>> keys) {
  DVLOG_IF(1, !success) << "FeedContentDatabase load content keys failed.";
  UMA_HISTOGRAM_BOOLEAN(
      "ContentSuggestions.Feed.ContentStorage.LoadKeysSuccess", success);

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
    DVLOG(1) << "FeedContentDatabase committed failed.";
    ReportOperationResult(false);
    std::move(callback).Run(success);
    return;
  }

  // All operations were committed successfully, call |callback|.
  if (content_mutation->Empty()) {
    ReportOperationResult(true);

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
