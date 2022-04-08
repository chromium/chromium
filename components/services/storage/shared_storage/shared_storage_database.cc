// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_database.h"

#include <inttypes.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sql/error_delegate_util.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

// Because each entry is a key-value pair, and both keys and values are
// std::u16strings and bounded by `max_string_length_`, the total bytes used per
// entry is at most 2 * 2 * `max_string_length_`.
const int kSharedStorageEntryTotalBytesMultiplier = 4;

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 1;

[[nodiscard]] std::string SerializeOrigin(const url::Origin& origin) {
  DCHECK(!origin.opaque());
  DCHECK_NE(url::kFileScheme, origin.scheme());
  return origin.Serialize();
}

[[nodiscard]] bool InitSchema(sql::Database& db) {
  static constexpr char kValuesMappingSql[] =
      "CREATE TABLE IF NOT EXISTS values_mapping("
      "context_origin TEXT NOT NULL,"
      "key TEXT NOT NULL,"
      "value TEXT,"
      "PRIMARY KEY(context_origin,key)) WITHOUT ROWID";
  if (!db.Execute(kValuesMappingSql))
    return false;

  static constexpr char kPerOriginMappingSql[] =
      "CREATE TABLE IF NOT EXISTS per_origin_mapping("
      "context_origin TEXT NOT NULL PRIMARY KEY,"
      "last_used_time INTEGER NOT NULL,"
      "length INTEGER NOT NULL) WITHOUT ROWID";
  if (!db.Execute(kPerOriginMappingSql))
    return false;

  static constexpr char kLastUsedTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS per_origin_mapping_last_used_time_idx "
      "ON per_origin_mapping(last_used_time)";
  if (!db.Execute(kLastUsedTimeIndexSql))
    return false;

  return true;
}

}  // namespace

SharedStorageDatabase::GetResult::GetResult() = default;

SharedStorageDatabase::GetResult::GetResult(const GetResult&) = default;

SharedStorageDatabase::GetResult::GetResult(GetResult&&) = default;

SharedStorageDatabase::GetResult::~GetResult() = default;

SharedStorageDatabase::GetResult& SharedStorageDatabase::GetResult::operator=(
    const GetResult&) = default;

SharedStorageDatabase::GetResult& SharedStorageDatabase::GetResult::operator=(
    GetResult&&) = default;

SharedStorageDatabase::SharedStorageDatabase(
    base::FilePath db_path,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<SharedStorageDatabaseOptions> options)
    : db_({// Run the database in exclusive mode. Nobody else should be
           // accessing the database while we're running, and this will give
           // somewhat improved perf.
           .exclusive_locking = true,
           .page_size = options->max_page_size,
           .cache_size = options->max_cache_size}),
      db_path_(std::move(db_path)),
      special_storage_policy_(std::move(special_storage_policy)),
      max_entries_per_origin_(int64_t{options->max_entries_per_origin}),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK(db_path_.empty() || db_path_.IsAbsolute());
  DCHECK_GT(max_entries_per_origin_, 0);
  DCHECK_GT(options->max_init_tries, 0);
  DCHECK_GT(options->max_string_length, 0);
  DCHECK_GT(options->max_iterator_batch_size, 0);
  max_string_length_ = static_cast<size_t>(options->max_string_length);
  max_init_tries_ = static_cast<size_t>(options->max_init_tries);
  max_iterator_batch_size_ =
      static_cast<size_t>(options->max_iterator_batch_size);
  db_file_status_ = db_path_.empty() ? DBFileStatus::kNoPreexistingFile
                                     : DBFileStatus::kNotChecked;
}

SharedStorageDatabase::~SharedStorageDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool SharedStorageDatabase::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open() && !db_.RazeAndClose())
    return false;

  // The file already doesn't exist.
  if (db_path_.empty())
    return true;

  return base::DeleteFile(db_path_);
}

void SharedStorageDatabase::TrimMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(InitStatus::kSuccess, db_status_);
  db_.TrimMemory();
}

SharedStorageDatabase::GetResult SharedStorageDatabase::Get(
    url::Origin context_origin,
    std::u16string key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(key.size(), max_string_length_);
  GetResult result;

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      result.result = OperationResult::kSuccess;
    else
      result.result = OperationResult::kInitFailure;

    return result;
  }

  // In theory, there ought to be at most one entry found. But we make no
  // assumption about the state of the disk. In the rare case that multiple
  // entries are found, we return only the value from the first entry found.
  static constexpr char kSelectSql[] =
      "SELECT value FROM values_mapping "
      "WHERE context_origin=? AND key=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  std::string origin_str(SerializeOrigin(context_origin));
  statement.BindString(0, origin_str);
  statement.BindString16(1, key);

  if (statement.Step())
    result.data = statement.ColumnString16(0);
  if (!statement.Succeeded())
    return result;

  if (UpdateLastUsedTime(origin_str))
    result.result = OperationResult::kSuccess;

  return result;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Set(
    url::Origin context_origin,
    std::u16string key,
    std::u16string value,
    SetBehavior behavior) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!key.empty());
  DCHECK_LE(key.size(), max_string_length_);
  DCHECK_LE(value.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kCreateIfAbsent) != InitStatus::kSuccess)
    return OperationResult::kInitFailure;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  std::string origin_str(SerializeOrigin(context_origin));
  if (HasEntryFor(origin_str, key)) {
    if (behavior == SharedStorageDatabase::SetBehavior::kIgnoreIfPresent) {
      // If we are in a nested transaction, we need to commit, even though we
      // haven't made any changes, so that the failure to set in this case
      // isn't seen as an error (as then the entire stack of transactions
      // will be rolled back and the next transaction within the parent
      // transaction will fail to begin).
      if (db_.transaction_nesting())
        transaction.Commit();
      return OperationResult::kIgnored;
    }

    if (Delete(context_origin, key) != OperationResult::kSuccess)
      return OperationResult::kSqlError;
  } else if (!HasCapacity(origin_str)) {
    return OperationResult::kNoCapacity;
  }

  if (!InsertIntoValuesMapping(origin_str, key, value))
    return OperationResult::kSqlError;

  if (!UpdateLength(origin_str, /*delta=*/1))
    return OperationResult::kSqlError;

  if (!transaction.Commit())
    return OperationResult::kSqlError;

  return OperationResult::kSet;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Append(
    url::Origin context_origin,
    std::u16string key,
    std::u16string tail_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!key.empty());
  DCHECK_LE(key.size(), max_string_length_);
  DCHECK_LE(tail_value.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kCreateIfAbsent) != InitStatus::kSuccess)
    return OperationResult::kInitFailure;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  GetResult get_result = Get(context_origin, key);
  if (get_result.result != OperationResult::kSuccess)
    return OperationResult::kSqlError;

  std::u16string new_value;
  std::string origin_str(SerializeOrigin(context_origin));

  if (get_result.data) {
    new_value = std::move(*get_result.data);
    new_value.append(tail_value);

    if (new_value.size() > max_string_length_)
      return OperationResult::kInvalidAppend;

    if (Delete(context_origin, key) != OperationResult::kSuccess)
      return OperationResult::kSqlError;
  } else {
    new_value = std::move(tail_value);

    if (!HasCapacity(origin_str))
      return OperationResult::kNoCapacity;
  }

  if (!InsertIntoValuesMapping(origin_str, key, new_value))
    return OperationResult::kSqlError;

  if (!UpdateLength(origin_str, /*delta=*/1))
    return OperationResult::kSqlError;

  if (!transaction.Commit())
    return OperationResult::kSqlError;

  return OperationResult::kSet;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Delete(
    url::Origin context_origin,
    std::u16string key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(key.size(), max_string_length_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  std::string origin_str(SerializeOrigin(context_origin));
  if (!HasEntryFor(origin_str, key))
    return OperationResult::kSuccess;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  static constexpr char kDeleteSql[] =
      "DELETE FROM values_mapping "
      "WHERE context_origin=? AND key=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, origin_str);
  statement.BindString16(1, key);

  if (!statement.Run())
    return OperationResult::kSqlError;

  if (!UpdateLength(origin_str, /*delta=*/-1))
    return OperationResult::kSqlError;

  if (!transaction.Commit())
    return OperationResult::kSqlError;
  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Clear(
    url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  if (!Purge(SerializeOrigin(context_origin)))
    return OperationResult::kSqlError;
  return OperationResult::kSuccess;
}

int64_t SharedStorageDatabase::Length(url::Origin context_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return -1 (to signifiy an error) if the database doesn't exist,
    // but only if it pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return 0L;
    else
      return -1;
  }

  std::string origin_str(SerializeOrigin(context_origin));
  int64_t length = NumEntries(origin_str);
  if (!length)
    return 0L;

  if (!UpdateLastUsedTime(origin_str))
    return -1;

  return length;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Keys(
    const url::Origin& context_origin,
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<shared_storage_worklet::mojom::SharedStorageEntriesListener>
      keys_listener(std::move(pending_listener));

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted) {
      keys_listener->DidReadEntries(
          /*success=*/true,
          /*error_message=*/"", /*entries=*/{}, /*has_more_entries=*/false);
      return OperationResult::kSuccess;
    } else {
      keys_listener->DidReadEntries(
          /*success=*/false, "SQL database had initialization failure.",
          /*entries=*/{}, /*has_more_entries=*/false);
      return OperationResult::kInitFailure;
    }
  }

  static constexpr char kSelectSql[] =
      "SELECT key FROM values_mapping WHERE context_origin=? ORDER BY key";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  std::string origin_str(SerializeOrigin(context_origin));
  statement.BindString(0, origin_str);

  bool has_more_entries = true;
  absl::optional<std::u16string> saved_first_key_for_next_batch;

  while (has_more_entries) {
    has_more_entries = false;
    std::vector<shared_storage_worklet::mojom::SharedStorageKeyAndOrValuePtr>
        keys;

    if (saved_first_key_for_next_batch) {
      keys.push_back(
          shared_storage_worklet::mojom::SharedStorageKeyAndOrValue::New(
              saved_first_key_for_next_batch.value(), u""));
      saved_first_key_for_next_batch.reset();
    }

    while (statement.Step()) {
      if (keys.size() < max_iterator_batch_size_) {
        keys.push_back(
            shared_storage_worklet::mojom::SharedStorageKeyAndOrValue::New(
                statement.ColumnString16(0), u""));
      } else {
        // Cache the current key to use as the start of the next batch, as we're
        // already passing through this step and the next iteration of
        // `statement.Step()`, if there is one, during the next iteration of the
        // outer while loop, will give us the subsequent key.
        saved_first_key_for_next_batch = statement.ColumnString16(0);
        has_more_entries = true;
        break;
      }
    }

    if (!statement.Succeeded()) {
      keys_listener->DidReadEntries(
          /*success=*/false,
          "SQL database encountered an error while retrieving keys.",
          /*entries=*/{}, /*has_more_entries=*/false);
      return OperationResult::kSqlError;
    }

    keys_listener->DidReadEntries(/*success=*/true, /*error_message=*/"",
                                  std::move(keys), has_more_entries);
  }

  if (!UpdateLastUsedTime(origin_str))
    return OperationResult::kSqlError;

  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::Entries(
    const url::Origin& context_origin,
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<shared_storage_worklet::mojom::SharedStorageEntriesListener>
      entries_listener(std::move(pending_listener));

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted) {
      entries_listener->DidReadEntries(
          /*success=*/true,
          /*error_message=*/"", /*entries=*/{}, /*has_more_entries=*/false);
      return OperationResult::kSuccess;
    } else {
      entries_listener->DidReadEntries(
          /*success=*/false, "SQL database had initialization failure.",
          /*entries=*/{}, /*has_more_entries=*/false);
      return OperationResult::kInitFailure;
    }
  }

  static constexpr char kSelectSql[] =
      "SELECT key,value FROM values_mapping WHERE context_origin=? "
      "ORDER BY key";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  std::string origin_str(SerializeOrigin(context_origin));
  statement.BindString(0, origin_str);

  bool has_more_entries = true;
  absl::optional<std::u16string> saved_first_key_for_next_batch;
  absl::optional<std::u16string> saved_first_value_for_next_batch;

  while (has_more_entries) {
    has_more_entries = false;
    std::vector<shared_storage_worklet::mojom::SharedStorageKeyAndOrValuePtr>
        entries;

    if (saved_first_key_for_next_batch) {
      DCHECK(saved_first_value_for_next_batch);
      entries.push_back(
          shared_storage_worklet::mojom::SharedStorageKeyAndOrValue::New(
              saved_first_key_for_next_batch.value(),
              saved_first_value_for_next_batch.value()));
      saved_first_key_for_next_batch.reset();
      saved_first_value_for_next_batch.reset();
    }

    while (statement.Step()) {
      if (entries.size() < max_iterator_batch_size_) {
        entries.push_back(
            shared_storage_worklet::mojom::SharedStorageKeyAndOrValue::New(
                statement.ColumnString16(0), statement.ColumnString16(1)));
      } else {
        // Cache the current key and value to use as the start of the next
        // batch, as we're already passing through this step and the next
        // iteration of `statement.Step()`, if there is one, during the next
        // iteration of the outer while loop, will give us the subsequent
        // key-value pair.
        saved_first_key_for_next_batch = statement.ColumnString16(0);
        saved_first_value_for_next_batch = statement.ColumnString16(1);
        has_more_entries = true;
        break;
      }
    }

    if (!statement.Succeeded()) {
      entries_listener->DidReadEntries(
          /*success=*/false,
          "SQL database encountered an error while retrieving entries.",
          /*entries=*/{}, /*has_more_entries=*/false);
      return OperationResult::kSqlError;
    }

    entries_listener->DidReadEntries(/*success=*/true, /*error_message=*/"",
                                     std::move(entries), has_more_entries);
  }

  if (!UpdateLastUsedTime(origin_str))
    return OperationResult::kSqlError;

  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult
SharedStorageDatabase::PurgeMatchingOrigins(
    OriginMatcherFunction origin_matcher,
    base::Time begin,
    base::Time end,
    bool perform_storage_cleanup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(begin, end);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  static constexpr char kSelectSql[] =
      "SELECT context_origin FROM per_origin_mapping "
      "WHERE last_used_time BETWEEN ? AND ? "
      "ORDER BY last_used_time";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindTime(0, begin);
  statement.BindTime(1, end);

  std::vector<std::string> origins;

  while (statement.Step())
    origins.push_back(statement.ColumnString(0));

  if (!statement.Succeeded())
    return OperationResult::kSqlError;

  if (origins.empty())
    return OperationResult::kSuccess;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  for (const auto& origin : origins) {
    if (origin_matcher && !origin_matcher.Run(url::Origin::Create(GURL(origin)),
                                              special_storage_policy_.get())) {
      continue;
    }

    if (!Purge(origin))
      return OperationResult::kSqlError;
  }

  if (!transaction.Commit())
    return OperationResult::kSqlError;

  if (perform_storage_cleanup && !Vacuum())
    return OperationResult::kSqlError;

  return OperationResult::kSuccess;
}

SharedStorageDatabase::OperationResult SharedStorageDatabase::PurgeStaleOrigins(
    base::TimeDelta window_to_be_deemed_active) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(window_to_be_deemed_active, base::TimeDelta());

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess) {
    // We do not return an error if the database doesn't exist, but only if it
    // pre-exists on disk and yet fails to initialize.
    if (db_status_ == InitStatus::kUnattempted)
      return OperationResult::kSuccess;
    else
      return OperationResult::kInitFailure;
  }

  base::Time threshold = clock_->Now() - window_to_be_deemed_active;

  static constexpr char kSelectSql[] =
      "SELECT context_origin FROM per_origin_mapping "
      "WHERE last_used_time<? "
      "ORDER BY last_used_time";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindTime(0, threshold);

  std::vector<std::string> stale_origins;

  while (statement.Step())
    stale_origins.push_back(statement.ColumnString(0));

  if (!statement.Succeeded())
    return OperationResult::kSqlError;

  if (stale_origins.empty())
    return OperationResult::kSuccess;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return OperationResult::kSqlError;

  for (const auto& origin : stale_origins) {
    if (!Purge(origin))
      return OperationResult::kSqlError;
  }

  if (!transaction.Commit())
    return OperationResult::kSqlError;
  return OperationResult::kSuccess;
}

std::vector<mojom::StorageUsageInfoPtr> SharedStorageDatabase::FetchOrigins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess)
    return {};

  static constexpr char kSelectSql[] =
      "SELECT context_origin,last_used_time,length FROM per_origin_mapping "
      "ORDER BY context_origin";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  std::vector<mojom::StorageUsageInfoPtr> fetched_origin_infos;

  while (statement.Step()) {
    fetched_origin_infos.emplace_back(mojom::StorageUsageInfo::New(
        url::Origin::Create(GURL(statement.ColumnString(0))),
        statement.ColumnInt64(2) * kSharedStorageEntryTotalBytesMultiplier *
            max_string_length_,
        statement.ColumnTime(1)));
  }

  if (!statement.Succeeded())
    return {};

  return fetched_origin_infos;
}

bool SharedStorageDatabase::IsOpenForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_.is_open();
}

SharedStorageDatabase::InitStatus SharedStorageDatabase::DBStatusForTesting()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_status_;
}

bool SharedStorageDatabase::OverrideLastUsedTimeForTesting(
    url::Origin context_origin,
    base::Time new_last_used_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (LazyInit(DBCreationPolicy::kIgnoreIfAbsent) != InitStatus::kSuccess)
    return false;

  return SetLastUsedTime(SerializeOrigin(context_origin), new_last_used_time);
}

void SharedStorageDatabase::OverrideClockForTesting(base::Clock* clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(clock);
  clock_ = clock;
}

void SharedStorageDatabase::OverrideSpecialStoragePolicyForTesting(
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  special_storage_policy_ = std::move(special_storage_policy);
}

bool SharedStorageDatabase::PopulateDatabaseForTesting(url::Origin origin1,
                                                       url::Origin origin2,
                                                       url::Origin origin3) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We use `CHECK_EQ()` and `CHECK()` macros instead of early returns because
  // the latter made the test coverage delta too low.
  CHECK_EQ(OperationResult::kSet,
           Set(origin1, u"key1", u"value1", SetBehavior::kDefault));

  CHECK_EQ(OperationResult::kSet,
           Set(origin1, u"key2", u"value1", SetBehavior::kDefault));

  CHECK_EQ(OperationResult::kSet,
           Set(origin2, u"key1", u"value2", SetBehavior::kDefault));

  CHECK(OverrideLastUsedTimeForTesting(  // IN-TEST
      origin2, clock_->Now() - base::Days(1)));

  CHECK_EQ(OperationResult::kSet,
           Set(origin3, u"key1", u"value1", SetBehavior::kDefault));

  CHECK_EQ(OperationResult::kSet,
           Set(origin3, u"key2", u"value2", SetBehavior::kDefault));

  CHECK(OverrideLastUsedTimeForTesting(  // IN-TEST
      origin3, clock_->Now() - base::Days(60)));

  // We return a bool in order to facilitate use of `base::test::TestFuture`
  // with this method.
  return true;
}

SharedStorageDatabase::InitStatus SharedStorageDatabase::LazyInit(
    DBCreationPolicy policy) {
  // Early return in case of previous failure, to prevent an unbounded
  // number of re-attempts.
  if (db_status_ != InitStatus::kUnattempted)
    return db_status_;

  if (policy == DBCreationPolicy::kIgnoreIfAbsent && !DBExists())
    return InitStatus::kUnattempted;

  for (size_t i = 0; i < max_init_tries_; ++i) {
    db_status_ = InitImpl();
    if (db_status_ == InitStatus::kSuccess)
      return db_status_;

    meta_table_.Reset();
    db_.Close();
  }

  return db_status_;
}

bool SharedStorageDatabase::DBExists() {
  DCHECK_EQ(InitStatus::kUnattempted, db_status_);

  if (db_file_status_ == DBFileStatus::kNoPreexistingFile)
    return false;

  // The in-memory case is included in `DBFileStatus::kNoPreexistingFile`.
  DCHECK(!db_path_.empty());

  // We do not expect `DBExists()` to be called in the case where
  // `db_file_status_ == DBFileStatus::kPreexistingFile`, as then
  // `db_status_ != InitStatus::kUnattempted`, which would force an early return
  // in `LazyInit()`.
  DCHECK_EQ(DBFileStatus::kNotChecked, db_file_status_);

  // The histogram tag must be set before opening.
  db_.set_histogram_tag("SharedStorage");

  if (!db_.Open(db_path_)) {
    db_file_status_ = DBFileStatus::kNoPreexistingFile;
    return false;
  }

  static const char kSelectSql[] =
      "SELECT COUNT(*) FROM sqlite_schema WHERE type=?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindCString(0, "table");

  if (!statement.Step() || statement.ColumnInt(0) == 0) {
    db_file_status_ = DBFileStatus::kNoPreexistingFile;
    return false;
  }

  db_file_status_ = DBFileStatus::kPreexistingFile;
  return true;
}

bool SharedStorageDatabase::OpenDatabase() {
  // If the database is open, the histogram tag will have already been set in
  // `DBExists()`, since it must be set before opening.
  if (!db_.is_open())
    db_.set_histogram_tag("SharedStorage");

  // base::Unretained is safe here because this SharedStorageDatabase owns
  // the sql::Database instance that stores and uses the callback. So,
  // `this` is guaranteed to outlive the callback.
  db_.set_error_callback(base::BindRepeating(
      &SharedStorageDatabase::DatabaseErrorCallback, base::Unretained(this)));

  if (!db_path_.empty()) {
    if (!db_.is_open() && !db_.Open(db_path_))
      return false;

    db_.Preload();
  } else {
    if (!db_.OpenInMemory())
      return false;
  }

  return true;
}

void SharedStorageDatabase::DatabaseErrorCallback(int extended_error,
                                                  sql::Statement* stmt) {
  base::UmaHistogramSparse("Storage.SharedStorage.Database.Error",
                           extended_error);

  if (sql::IsErrorCatastrophic(extended_error)) {
    bool success = Destroy();
    base::UmaHistogramBoolean("Storage.SharedStorage.Database.Destruction",
                              success);
    if (!success) {
      DLOG(FATAL) << "Database destruction failed after catastrophic error:\n"
                  << db_.GetErrorMessage();
    }
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db_.GetErrorMessage();
}

SharedStorageDatabase::InitStatus SharedStorageDatabase::InitImpl() {
  if (!OpenDatabase())
    return InitStatus::kError;

  // Database should now be open.
  DCHECK(db_.is_open());

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    LOG(WARNING) << "Shared storage database begin initialization failed.";
    db_.RazeAndClose();
    return InitStatus::kError;
  }

  // Create the tables.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCurrentVersionNumber) ||
      !InitSchema(db_)) {
    return InitStatus::kError;
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Shared storage database is too new.";
    return InitStatus::kTooNew;
  }

  int cur_version = meta_table_.GetVersionNumber();

  if (cur_version < kCurrentVersionNumber) {
    LOG(WARNING) << "Shared storage database is too old to be compatible.";
    db_.RazeAndClose();
    return InitStatus::kTooOld;
  }

  // The initialization is complete.
  if (!transaction.Commit()) {
    LOG(WARNING) << "Shared storage database initialization commit failed.";
    db_.RazeAndClose();
    return InitStatus::kError;
  }

  return InitStatus::kSuccess;
}

bool SharedStorageDatabase::Vacuum() {
  DCHECK_EQ(InitStatus::kSuccess, db_status_);
  DCHECK_EQ(0, db_.transaction_nesting())
      << "Can not have a transaction when vacuuming.";
  return db_.Execute("VACUUM");
}

bool SharedStorageDatabase::Purge(const std::string& context_origin) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteSql[] =
      "DELETE FROM values_mapping "
      "WHERE context_origin=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, context_origin);

  if (!statement.Run())
    return false;

  if (!DeleteFromPerOriginMapping(context_origin))
    return false;

  return transaction.Commit();
}

int64_t SharedStorageDatabase::NumEntries(const std::string& context_origin) {
  // In theory, there ought to be at most one entry found. But we make no
  // assumption about the state of the disk. In the rare case that multiple
  // entries are found, we return only the `length` from the first entry found.
  static constexpr char kSelectSql[] =
      "SELECT length FROM per_origin_mapping "
      "WHERE context_origin=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, context_origin);

  int64_t length = 0;
  if (statement.Step())
    length = statement.ColumnInt64(0);

  return length;
}

bool SharedStorageDatabase::HasEntryFor(const std::string& context_origin,
                                        const std::u16string& key) {
  static constexpr char kSelectSql[] =
      "SELECT 1 FROM values_mapping "
      "WHERE context_origin=? AND key=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, context_origin);
  statement.BindString16(1, key);

  return statement.Step();
}

bool SharedStorageDatabase::SetLastUsedTime(const std::string& context_origin,
                                            base::Time new_last_used_time) {
  int64_t length = NumEntries(context_origin);

  // If length is zero, no need to delete, and don't insert the origin into the
  // `per_origin_mapping`.
  if (!length)
    return true;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  if (!DeleteFromPerOriginMapping(context_origin))
    return false;

  if (!InsertIntoPerOriginMapping(context_origin, new_last_used_time, length))
    return false;

  return transaction.Commit();
}

bool SharedStorageDatabase::UpdateLastUsedTime(
    const std::string& context_origin) {
  return SetLastUsedTime(context_origin, clock_->Now());
}

bool SharedStorageDatabase::UpdateLength(const std::string& context_origin,
                                         int64_t delta,
                                         bool should_update_time) {
  // In theory, there ought to be at most one entry found. But we make no
  // assumption about the state of the disk. In the rare case that multiple
  // entries are found, we retrieve only the `length` (and possibly the `time`)
  // from the first entry found.
  static constexpr char kSelectSql[] =
      "SELECT length,last_used_time FROM per_origin_mapping "
      "WHERE context_origin=? "
      "LIMIT 1";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, context_origin);
  int64_t length = 0;
  base::Time time = clock_->Now();

  if (statement.Step()) {
    length = statement.ColumnInt64(0);
    if (!should_update_time)
      time = statement.ColumnTime(1);
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  if (!DeleteFromPerOriginMapping(context_origin))
    return false;

  // If the new length is zero, then don't re-insert the origin into the
  // `per_origin_mapping`.
  if (length + delta == 0L)
    return transaction.Commit();

  if (!InsertIntoPerOriginMapping(context_origin, time, length + delta))
    return false;

  return transaction.Commit();
}

bool SharedStorageDatabase::InsertIntoValuesMapping(
    const std::string& context_origin,
    const std::u16string& key,
    const std::u16string& value) {
  static constexpr char kInsertSql[] =
      "INSERT INTO values_mapping(context_origin,key,value)"
      "VALUES(?,?,?)";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, context_origin);
  statement.BindString16(1, key);
  statement.BindString16(2, value);

  return statement.Run();
}

bool SharedStorageDatabase::DeleteFromPerOriginMapping(
    const std::string& context_origin) {
  static constexpr char kDeleteSql[] =
      "DELETE FROM per_origin_mapping "
      "WHERE context_origin=?";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, context_origin);

  return statement.Run();
}

bool SharedStorageDatabase::InsertIntoPerOriginMapping(
    const std::string& context_origin,
    base::Time last_used_time,
    uint64_t length) {
  static constexpr char kInsertSql[] =
      "INSERT INTO per_origin_mapping(context_origin,last_used_time,length)"
      "VALUES(?,?,?)";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, context_origin);
  statement.BindTime(1, last_used_time);
  statement.BindInt64(2, static_cast<int64_t>(length));

  return statement.Run();
}

bool SharedStorageDatabase::HasCapacity(const std::string& context_origin) {
  return NumEntries(context_origin) < max_entries_per_origin_;
}

}  // namespace storage
