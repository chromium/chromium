// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/strike_database.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace autofill {

namespace {
const int kMaxInitAttempts = 3;
}  // namespace

const base::FilePath::StringPieceType kStrikeDatabaseFileName =
    FILE_PATH_LITERAL("AutofillStrikeDatabase");

StrikeDatabase::StrikeDatabase(
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    base::FilePath profile_path) {
  const auto strike_database_path =
      profile_path.Append(kStrikeDatabaseFileName);

  const auto database_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  db_ = db_provider->GetDB<StrikeData>(
      leveldb_proto::ProtoDbType::STRIKE_DATABASE, strike_database_path,
      database_task_runner);

  db_->Init(base::BindRepeating(&StrikeDatabase::OnDatabaseInit,
                                weak_ptr_factory_.GetWeakPtr()));
}

StrikeDatabase::~StrikeDatabase() = default;

int StrikeDatabase::AddStrikes(int strikes_increase, const std::string& key) {
  DCHECK_GT(strikes_increase, 0);
  int num_strikes =
      strike_map_cache_.count(key)  // Cache has entry for |key|.
          ? strike_map_cache_[key].num_strikes() + strikes_increase
          : strikes_increase;
  SetStrikeData(key, num_strikes);
  return num_strikes;
}

int StrikeDatabase::RemoveStrikes(int strikes_decrease,
                                  const std::string& key) {
  int num_strikes = GetStrikes(key);
  num_strikes = std::max(0, num_strikes - strikes_decrease);
  SetStrikeData(key, num_strikes);
  return num_strikes;
}

int StrikeDatabase::GetStrikes(const std::string& key) {
  auto iter = strike_map_cache_.find(key);
  return (iter != strike_map_cache_.end()) ? iter->second.num_strikes() : 0;
}

void StrikeDatabase::ClearStrikes(const std::string& key) {
  strike_map_cache_.erase(key);
  ClearAllProtoStrikesForKey(key, base::DoNothing());
}

std::map<std::string, StrikeData>& StrikeDatabase::GetStrikeCache() {
  return strike_map_cache_;
}

void StrikeDatabase::SetStrikeData(const std::string& key, int num_strikes) {
  if (num_strikes == 0) {
    ClearStrikes(key);
    return;
  }
  StrikeData data;
  data.set_num_strikes(num_strikes);
  data.set_last_update_timestamp(
      AutofillClock::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  UpdateCache(key, data);
  SetProtoStrikeData(key, data, base::DoNothing());
}

int64_t StrikeDatabase::GetLastUpdatedTimestamp(const std::string& key) {
  auto iter = strike_map_cache_.find(key);
  return (iter != strike_map_cache_.end())
             ? iter->second.last_update_timestamp()
             : 0;
}

std::vector<std::string> StrikeDatabase::GetAllStrikeKeysForProject(
    const std::string& project_prefix) {
  std::vector<std::string> project_keys;
  for (const auto& [key, data] : strike_map_cache_) {
    if (key.find(project_prefix) == 0) {
      project_keys.push_back(key);
    }
  }
  return project_keys;
}

void StrikeDatabase::ClearAllStrikesForProject(
    const std::string& project_prefix) {
  ClearStrikesForKeys(GetAllStrikeKeysForProject(project_prefix));
}

void StrikeDatabase::ClearStrikesForKeys(
    const std::vector<std::string>& keys_to_remove) {
  for (const auto& key : keys_to_remove) {
    strike_map_cache_.erase(key);
  }
  ClearAllProtoStrikesForKeys(keys_to_remove, base::DoNothing());
}

void StrikeDatabase::ClearAllStrikes() {
  strike_map_cache_.clear();
  ClearAllProtoStrikes(base::DoNothing());
}

std::string StrikeDatabase::GetPrefixFromKey(const std::string& key) const {
  return key.substr(0, key.find(KeyDeliminator()));
}

StrikeDatabase::StrikeDatabase() : db_(nullptr) {}

void StrikeDatabase::OnDatabaseInit(leveldb_proto::Enums::InitStatus status) {
  bool success = status == leveldb_proto::Enums::InitStatus::kOK;
  database_initialized_ = success;
  if (!success) {
    base::UmaHistogramCounts100(
        "Autofill.StrikeDatabase.StrikeDatabaseInitFailed", num_init_attempts_);
    if (num_init_attempts_ < kMaxInitAttempts) {
      num_init_attempts_++;
      db_->Init(base::BindRepeating(&StrikeDatabase::OnDatabaseInit,
                                    weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }
  db_->LoadKeysAndEntries(
      base::BindRepeating(&StrikeDatabase::OnDatabaseLoadKeysAndEntries,
                          weak_ptr_factory_.GetWeakPtr()));
}

void StrikeDatabase::OnDatabaseLoadKeysAndEntries(
    bool success,
    std::unique_ptr<std::map<std::string, StrikeData>> entries) {
  if (!success) {
    database_initialized_ = false;
    return;
  }
  strike_map_cache_.insert(entries->begin(), entries->end());
}

void StrikeDatabase::GetProtoStrikes(const std::string& key,
                                     const StrikesCallback& outer_callback) {
  if (!database_initialized_) {
    outer_callback.Run(false);
    return;
  }
  GetProtoStrikeData(key,
                     base::BindRepeating(&StrikeDatabase::OnGetProtoStrikes,
                                         std::move(outer_callback)));
}

void StrikeDatabase::ClearAllProtoStrikes(
    const ClearStrikesCallback& outer_callback) {
  if (!database_initialized_) {
    outer_callback.Run(false);
    return;
  }
  // For deleting all, filter method always returns true.
  db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<StrikeDataProto::KeyEntryVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      outer_callback);
}

void StrikeDatabase::ClearAllProtoStrikesForKeys(
    const std::vector<std::string>& keys,
    const ClearStrikesCallback& outer_callback) {
  if (!database_initialized_) {
    outer_callback.Run(false);
    return;
  }
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  *keys_to_remove = keys;
  db_->UpdateEntries(
      /*entries_to_save=*/std::make_unique<
          leveldb_proto::ProtoDatabase<StrikeData>::KeyEntryVector>(),
      /*keys_to_remove=*/std::move(keys_to_remove), outer_callback);
}

void StrikeDatabase::ClearAllProtoStrikesForKey(
    const std::string& key,
    const ClearStrikesCallback& outer_callback) {
  std::vector<std::string> keys_to_delete({key});
  ClearAllProtoStrikesForKeys(keys_to_delete, outer_callback);
}

void StrikeDatabase::GetProtoStrikeData(const std::string& key,
                                        const GetValueCallback& callback) {
  if (!database_initialized_) {
    callback.Run(false, nullptr);
    return;
  }
  db_->GetEntry(key, callback);
}

void StrikeDatabase::SetProtoStrikeData(const std::string& key,
                                        const StrikeData& data,
                                        const SetValueCallback& callback) {
  if (!database_initialized_) {
    callback.Run(false);
    return;
  }
  std::unique_ptr<StrikeDataProto::KeyEntryVector> entries(
      new StrikeDataProto::KeyEntryVector());
  entries->push_back(std::make_pair(key, data));
  db_->UpdateEntries(
      /*entries_to_save=*/std::move(entries),
      /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(),
      callback);
}

// static
void StrikeDatabase::OnGetProtoStrikes(
    StrikesCallback callback,
    bool success,
    std::unique_ptr<StrikeData> strike_data) {
  if (success && strike_data) {
    callback.Run(strike_data->num_strikes());
  } else {
    callback.Run(0);
  }
}

void StrikeDatabase::LoadKeys(const LoadKeysCallback& callback) {
  db_->LoadKeys(callback);
}

void StrikeDatabase::UpdateCache(const std::string& key,
                                 const StrikeData& data) {
  strike_map_cache_[key] = data;
}

}  // namespace autofill
