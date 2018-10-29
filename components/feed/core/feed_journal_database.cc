// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_journal_database.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feed/core/feed_journal_mutation.h"
#include "components/feed/core/feed_journal_operation.h"
#include "components/feed/core/proto/journal_storage.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace feed {

namespace {

using StorageEntryVector =
    leveldb_proto::ProtoDatabase<JournalStorageProto>::KeyEntryVector;

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.FeedJournalDatabase. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kJournalDatabaseUMAClientName[] = "FeedJournalDatabase";

const char kJournalDatabaseFolder[] = "journal";

const size_t kDatabaseWriteBufferSizeBytes = 512 * 1024;
const size_t kDatabaseWriteBufferSizeBytesForLowEndDevice = 128 * 1024;

void ReportLoadEntriesHistograms(bool success, base::TimeTicks start_time) {
  UMA_HISTOGRAM_BOOLEAN("ContentSuggestions.Feed.JournalStorage.LoadSuccess",
                        success);
  base::TimeDelta load_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("ContentSuggestions.Feed.JournalStorage.LoadTime",
                      load_time);
}

}  // namespace

FeedJournalDatabase::FeedJournalDatabase(const base::FilePath& database_folder)
    : FeedJournalDatabase(
          database_folder,
          std::make_unique<
              leveldb_proto::ProtoDatabaseImpl<JournalStorageProto>>(
              base::CreateSequencedTaskRunnerWithTraits(
                  {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                   base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}))) {}

FeedJournalDatabase::FeedJournalDatabase(
    const base::FilePath& database_folder,
    std::unique_ptr<leveldb_proto::ProtoDatabase<JournalStorageProto>>
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
      database_folder.AppendASCII(kJournalDatabaseFolder);
  storage_database_->Init(
      kJournalDatabaseUMAClientName, storage_folder, options,
      base::BindOnce(&FeedJournalDatabase::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

FeedJournalDatabase::~FeedJournalDatabase() = default;

bool FeedJournalDatabase::IsInitialized() const {
  return INITIALIZED == database_status_;
}

void FeedJournalDatabase::LoadJournal(const std::string& key,
                                      JournalLoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_database_->GetEntry(
      key, base::BindOnce(&FeedJournalDatabase::OnGetEntryForLoadJournal,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::TimeTicks::Now(), std::move(callback)));
}

void FeedJournalDatabase::DoesJournalExist(const std::string& key,
                                           CheckExistingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_database_->GetEntry(
      key, base::BindOnce(&FeedJournalDatabase::OnGetEntryForDoesJournalExist,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::TimeTicks::Now(), std::move(callback)));
}

void FeedJournalDatabase::CommitJournalMutation(
    std::unique_ptr<JournalMutation> journal_mutation,
    ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(journal_mutation);

  UMA_HISTOGRAM_COUNTS_100(
      "ContentSuggestions.Feed.JournalStorage.CommitMutationCount",
      journal_mutation->Size());

  if (journal_mutation->Empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }

  // Skip loading journal if the first operation is JOURNAL_DELETE.
  if (journal_mutation->FirstOperationType() ==
      JournalOperation::JOURNAL_DELETE) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FeedJournalDatabase::PerformOperations,
                       weak_ptr_factory_.GetWeakPtr(), nullptr,
                       std::move(journal_mutation), std::move(callback)));
    return;
  }

  std::string journal_name = journal_mutation->journal_name();
  storage_database_->GetEntry(
      journal_name,
      base::BindOnce(&FeedJournalDatabase::OnGetEntryForCommitJournalMutation,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(journal_mutation), std::move(callback)));
}

void FeedJournalDatabase::LoadAllJournalKeys(JournalLoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_database_->LoadKeys(
      base::BindOnce(&FeedJournalDatabase::OnLoadKeysForLoadAllJournalKeys,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(callback)));
}

void FeedJournalDatabase::DeleteAllJournals(ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
    JournalOperation operation = journal_mutation->TakeFristOperation();
    switch (operation.type()) {
      case JournalOperation::JOURNAL_APPEND:
        journal->add_journal_data(operation.value());
        break;
      case JournalOperation::JOURNAL_COPY:
        copy_to_journal[operation.to_journal_name()] =
            CopyJouarnal(operation.to_journal_name(), *journal);
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

  storage_database_->UpdateEntries(
      std::move(journals_to_save), std::move(journals_to_delete),
      base::BindOnce(&FeedJournalDatabase::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(start_time),
                     std::move(callback)));
}

void FeedJournalDatabase::OnDatabaseInitialized(bool success) {
  DCHECK_EQ(database_status_, UNINITIALIZED);

  if (success) {
    database_status_ = INITIALIZED;
  } else {
    database_status_ = INIT_FAILURE;
    DVLOG(1) << "FeedJournalDatabase init failed.";
  }
  UMA_HISTOGRAM_BOOLEAN("ContentSuggestions.Feed.JournalStorage.InitialSuccess",
                        success);
}

void FeedJournalDatabase::OnGetEntryForLoadJournal(
    base::TimeTicks start_time,
    JournalLoadCallback callback,
    bool success,
    std::unique_ptr<JournalStorageProto> journal) {
  DVLOG_IF(1, !success) << "FeedJournalDatabase load journal failed.";

  std::vector<std::string> results;
  if (journal) {
    for (int i = 0; i < journal->journal_data_size(); ++i) {
      results.emplace_back(journal->journal_data(i));
    }
  }

  ReportLoadEntriesHistograms(success, start_time);

  std::move(callback).Run(success, std::move(results));
}

void FeedJournalDatabase::OnGetEntryForDoesJournalExist(
    base::TimeTicks start_time,
    CheckExistingCallback callback,
    bool success,
    std::unique_ptr<JournalStorageProto> journal) {
  DVLOG_IF(1, !success) << "FeedJournalDatabase load journal failed.";

  ReportLoadEntriesHistograms(success, start_time);

  std::move(callback).Run(success, journal ? true : false);
}

void FeedJournalDatabase::OnLoadKeysForLoadAllJournalKeys(
    base::TimeTicks start_time,
    JournalLoadCallback callback,
    bool success,
    std::unique_ptr<std::vector<std::string>> keys) {
  DVLOG_IF(1, !success) << "FeedJournalDatabase load journal keys failed.";
  UMA_HISTOGRAM_BOOLEAN(
      "ContentSuggestions.Feed.JournalStorage.LoadKeysSuccess", success);

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
  DVLOG_IF(1, !success) << "FeedJournalDatabase commit failed.";
  UMA_HISTOGRAM_BOOLEAN(
      "ContentSuggestions.Feed.JournalStorage.OperationCommitSuccess", success);

  base::TimeDelta commit_time = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES(
      "ContentSuggestions.Feed.JournalStorage.OperationCommitTime",
      commit_time);

  std::move(callback).Run(success);
}

JournalStorageProto FeedJournalDatabase::CopyJouarnal(
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
