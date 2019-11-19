// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_journal_database.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feed/core/feed_journal_mutation.h"
#include "components/feed/core/feed_journal_operation.h"
#include "components/feed/core/proto/journal_storage.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace feed {

namespace {

const char kJournalDatabaseFolder[] = "journal";

// Journal updates happen infrequently and are typically ~8KB. However there's
// also one tiny write during startup before more writes that 1KB should handle.
const size_t kDatabaseWriteBufferSizeBytes = 1 * 1024;  // 1KB

void ReportLoadTimeHistogram(bool success, base::TimeTicks start_time) {
  base::TimeDelta load_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("ContentSuggestions.Feed.JournalStorage.LoadTime",
                      load_time);
}

}  // namespace

FeedJournalDatabase::FeedJournalDatabase(
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& database_folder)
    : database_status_(InitStatus::kNotInitialized),
      task_runner_(
          base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                           base::TaskPriority::USER_VISIBLE})),
      storage_database_(proto_database_provider->GetDB<JournalStorageProto>(
          leveldb_proto::ProtoDbType::FEED_JOURNAL_DATABASE,
          database_folder.AppendASCII(kJournalDatabaseFolder),
          task_runner_)) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&FeedJournalDatabase::InitInternal,
                                        weak_ptr_factory_.GetWeakPtr()));
}

// Used for testing.
FeedJournalDatabase::FeedJournalDatabase(
    std::unique_ptr<leveldb_proto::ProtoDatabase<JournalStorageProto>>
        storage_database,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : database_status_(InitStatus::kNotInitialized),
      task_runner_(task_runner),
      storage_database_(std::move(storage_database)) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&FeedJournalDatabase::InitInternal,
                                        weak_ptr_factory_.GetWeakPtr()));
}

FeedJournalDatabase::~FeedJournalDatabase() = default;

bool FeedJournalDatabase::IsInitialized() const {
  return database_status_ == InitStatus::kOK;
}

void FeedJournalDatabase::InitInternal() {
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  options.write_buffer_size = kDatabaseWriteBufferSizeBytes;

  storage_database_->Init(
      options, base::BindOnce(&FeedJournalDatabase::OnDatabaseInitialized,
                              weak_ptr_factory_.GetWeakPtr()));
}

void FeedJournalDatabase::LoadJournal(const std::string& key,
                                      JournalLoadCallback callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FeedJournalDatabase::GetEntryInternal,
          weak_ptr_factory_.GetWeakPtr(), std::move(key),
          base::BindOnce(&FeedJournalDatabase::OnGetEntryForLoadJournal,
                         weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                         std::move(callback))));
}

void FeedJournalDatabase::DoesJournalExist(const std::string& key,
                                           CheckExistingCallback callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FeedJournalDatabase::GetEntryInternal,
          weak_ptr_factory_.GetWeakPtr(), std::move(key),
          base::BindOnce(&FeedJournalDatabase::OnGetEntryForDoesJournalExist,
                         weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                         std::move(callback))));
}

void FeedJournalDatabase::CommitJournalMutation(
    std::unique_ptr<JournalMutation> journal_mutation,
    ConfirmationCallback callback) {
  DCHECK(journal_mutation);

  UMA_HISTOGRAM_COUNTS_100(
      "ContentSuggestions.Feed.JournalStorage.CommitMutationCount",
      journal_mutation->Size());

  if (journal_mutation->Empty()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), true));
    return;
  }

  // Skip loading journal if the first operation is JOURNAL_DELETE.
  if (journal_mutation->FirstOperationType() ==
      JournalOperation::JOURNAL_DELETE) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FeedJournalDatabase::PerformOperations,
                       weak_ptr_factory_.GetWeakPtr(), nullptr,
                       std::move(journal_mutation), std::move(callback)));
    return;
  }

  std::string journal_name = journal_mutation->journal_name();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FeedJournalDatabase::GetEntryInternal,
          weak_ptr_factory_.GetWeakPtr(), std::move(journal_name),
          base::BindOnce(
              &FeedJournalDatabase::OnGetEntryForCommitJournalMutation,
              weak_ptr_factory_.GetWeakPtr(), std::move(journal_mutation),
              std::move(callback))));
}

void FeedJournalDatabase::GetEntryInternal(
    const std::string& key,
    leveldb_proto::Callbacks::Internal<JournalStorageProto>::GetCallback
        callback) {
  storage_database_->GetEntry(key, std::move(callback));
}

void FeedJournalDatabase::LoadAllJournalKeys(JournalLoadCallback callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FeedJournalDatabase::LoadKeysInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedJournalDatabase::LoadKeysInternal(JournalLoadCallback callback) {
  storage_database_->LoadKeys(
      base::BindOnce(&FeedJournalDatabase::OnLoadKeysForLoadAllJournalKeys,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(callback)));
}

void FeedJournalDatabase::DeleteAllJournals(ConfirmationCallback callback) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FeedJournalDatabase::DeleteAllEntriesInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedJournalDatabase::DeleteAllEntriesInternal(
    ConfirmationCallback callback) {
  // For deleting all, filter method always return true.
  storage_database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<StorageEntryVector>(),
      base::BindRepeating([](const std::string& x) { return true; }),
      base::BindOnce(&FeedJournalDatabase::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(callback)));
}

void FeedJournalDatabase::PerformOperations(
    std::unique_ptr<JournalStorageProto> journal,
    std::unique_ptr<JournalMutation> journal_mutation,
    ConfirmationCallback callback) {
  DCHECK(!journal_mutation->Empty());

  if (journal) {
    DCHECK_EQ(journal->key(), journal_mutation->journal_name());
  } else {
    journal = std::make_unique<JournalStorageProto>();
    journal->set_key(journal_mutation->journal_name());
  }

  JournalMap copy_to_journal;
  while (!journal_mutation->Empty()) {
    JournalOperation operation = journal_mutation->TakeFirstOperation();
    switch (operation.type()) {
      case JournalOperation::JOURNAL_APPEND:
        journal->add_journal_data(operation.value());
        break;
      case JournalOperation::JOURNAL_COPY:
        copy_to_journal[operation.to_journal_name()] =
            CopyJournal(operation.to_journal_name(), *journal);
        break;
      case JournalOperation::JOURNAL_DELETE:
        journal->clear_journal_data();
        break;
    }
  }

  CommitOperations(journal_mutation->GetStartTime(), std::move(journal),
                   std::move(copy_to_journal), std::move(callback));
}

void FeedJournalDatabase::CommitOperations(
    base::TimeTicks start_time,
    std::unique_ptr<JournalStorageProto> journal,
    JournalMap copy_to_journal,
    ConfirmationCallback callback) {
  auto journals_to_save = std::make_unique<StorageEntryVector>();
  auto journals_to_delete = std::make_unique<std::vector<std::string>>();

  if (journal->journal_data_size() == 0) {
    // This can only happens when the journal is deleted.
    journals_to_delete->push_back(journal->key());
  } else {
    std::string journal_name = journal->key();
    journals_to_save->emplace_back(journal_name, std::move(*journal));
  }

  for (auto it = copy_to_journal.begin(); it != copy_to_journal.end(); ++it) {
    journals_to_save->emplace_back(it->first, std::move(it->second));
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FeedJournalDatabase::UpdateEntriesInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(journals_to_save), std::move(journals_to_delete),
                     std::move(start_time), std::move(callback)));
}

void FeedJournalDatabase::UpdateEntriesInternal(
    std::unique_ptr<StorageEntryVector> entries_to_save,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    base::TimeTicks start_time,
    ConfirmationCallback callback) {
  storage_database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::BindOnce(&FeedJournalDatabase::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(start_time),
                     std::move(callback)));
}

void FeedJournalDatabase::OnDatabaseInitialized(InitStatus status) {
  DCHECK_EQ(database_status_, InitStatus::kNotInitialized);
  database_status_ = status;
}

void FeedJournalDatabase::OnGetEntryForLoadJournal(
    base::TimeTicks start_time,
    JournalLoadCallback callback,
    bool success,
    std::unique_ptr<JournalStorageProto> journal) {
  std::vector<std::string> results;
  if (journal) {
    for (int i = 0; i < journal->journal_data_size(); ++i) {
      results.emplace_back(journal->journal_data(i));
    }
  }

  ReportLoadTimeHistogram(success, start_time);

  std::move(callback).Run(success, std::move(results));
}

void FeedJournalDatabase::OnGetEntryForDoesJournalExist(
    base::TimeTicks start_time,
    CheckExistingCallback callback,
    bool success,
    std::unique_ptr<JournalStorageProto> journal) {
  ReportLoadTimeHistogram(success, start_time);

  std::move(callback).Run(success, journal ? true : false);
}

void FeedJournalDatabase::OnLoadKeysForLoadAllJournalKeys(
    base::TimeTicks start_time,
    JournalLoadCallback callback,
    bool success,
    std::unique_ptr<std::vector<std::string>> keys) {
  std::vector<std::string> results;
  if (keys) {
    results = std::move(*keys);
  }

  if (success) {
    // Journal count is about how many Feed surfaces opens/shows to a user.
    UMA_HISTOGRAM_EXACT_LINEAR("ContentSuggestions.Feed.JournalStorage.Count",
                               results.size(), 50);
  }

  base::TimeDelta load_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("ContentSuggestions.Feed.JournalStorage.LoadKeysTime",
                      load_time);

  std::move(callback).Run(success, std::move(results));
}

void FeedJournalDatabase::OnGetEntryForCommitJournalMutation(
    std::unique_ptr<JournalMutation> journal_mutation,
    ConfirmationCallback callback,
    bool success,
    std::unique_ptr<JournalStorageProto> journal) {
  if (!success) {
    DVLOG(1) << "FeedJournalDatabase load journal failed.";
    std::move(callback).Run(success);
    return;
  }

  PerformOperations(std::move(journal), std::move(journal_mutation),
                    std::move(callback));
}

void FeedJournalDatabase::OnOperationCommitted(base::TimeTicks start_time,
                                               ConfirmationCallback callback,
                                               bool success) {
  base::TimeDelta commit_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES(
      "ContentSuggestions.Feed.JournalStorage.OperationCommitTime",
      commit_time);

  std::move(callback).Run(success);
}

JournalStorageProto FeedJournalDatabase::CopyJournal(
    const std::string& new_journal_name,
    const JournalStorageProto& source_journal) {
  JournalStorageProto new_journal;
  new_journal.set_key(new_journal_name);
  for (int i = 0; i < source_journal.journal_data_size(); ++i) {
    new_journal.add_journal_data(source_journal.journal_data(i));
  }

  return new_journal;
}

}  // namespace feed
