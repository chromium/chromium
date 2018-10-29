// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_database.h"

#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace autofill {

namespace {
const char kDatabaseClientName[] = "StrikeService";
const char kKeyDeliminator[] = "__";
const char kKeyPrefixForCreditCardSave[] = "creditCardSave";
}  // namespace

StrikeDatabase::StrikeDatabase(const base::FilePath& database_dir)
    : db_(std::make_unique<leveldb_proto::ProtoDatabaseImpl<StrikeData>>(
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}))),
      weak_ptr_factory_(this) {
  db_->Init(kDatabaseClientName, database_dir,
            leveldb_proto::CreateSimpleOptions(),
            base::BindRepeating(&StrikeDatabase::OnDatabaseInit,
                                weak_ptr_factory_.GetWeakPtr()));
}

StrikeDatabase::~StrikeDatabase() {}

void StrikeDatabase::GetStrikes(const std::string key,
                                const StrikesCallback& outer_callback) {
  GetStrikeData(key, base::BindRepeating(&StrikeDatabase::OnGetStrikes,
                                         base::Unretained(this),
                                         std::move(outer_callback)));
}

void StrikeDatabase::AddStrike(const std::string key,
                               const StrikesCallback& outer_callback) {
  GetStrikeData(key, base::BindRepeating(&StrikeDatabase::OnAddStrike,
                                         base::Unretained(this),
                                         std::move(outer_callback), key));
}

void StrikeDatabase::ClearAllStrikes(
    const ClearStrikesCallback& outer_callback) {
  // For deleting all, filter method always returns true.
  db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<StrikeDataProto::KeyEntryVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      base::BindRepeating(&StrikeDatabase::OnClearAllStrikes,
                          base::Unretained(this), outer_callback));
}

void StrikeDatabase::ClearAllStrikesForKey(
    const std::string& key,
    const ClearStrikesCallback& outer_callback) {
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  keys_to_remove->push_back(key);
  db_->UpdateEntries(
      /*entries_to_save=*/std::make_unique<
          leveldb_proto::ProtoDatabase<StrikeData>::KeyEntryVector>(),
      /*keys_to_remove=*/std::move(keys_to_remove),
      base::BindRepeating(&StrikeDatabase::OnClearAllStrikesForKey,
                          base::Unretained(this), outer_callback));
}

std::string StrikeDatabase::GetKeyForCreditCardSave(
    const std::string& card_last_four_digits) {
  return CreateKey(GetKeyPrefixForCreditCardSave(), card_last_four_digits);
}

StrikeDatabase::StrikeDatabase() : db_(nullptr), weak_ptr_factory_(this) {}

void StrikeDatabase::OnDatabaseInit(bool success) {}

void StrikeDatabase::GetStrikeData(const std::string key,
                                   const GetValueCallback& callback) {
  db_->GetEntry(key, callback);
}

void StrikeDatabase::SetStrikeData(const std::string& key,
                                   const StrikeData& data,
                                   const SetValueCallback& callback) {
  std::unique_ptr<StrikeDataProto::KeyEntryVector> entries(
      new StrikeDataProto::KeyEntryVector());
  entries->push_back(std::make_pair(key, data));
  db_->UpdateEntries(
      /*entries_to_save=*/std::move(entries),
      /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(),
      callback);
}

void StrikeDatabase::OnGetStrikes(StrikesCallback callback,
                                  bool success,
                                  std::unique_ptr<StrikeData> strike_data) {
  if (success && strike_data)
    callback.Run(strike_data->num_strikes());
  else
    callback.Run(0);
}

void StrikeDatabase::OnAddStrike(StrikesCallback callback,
                                 std::string key,
                                 bool success,
                                 std::unique_ptr<StrikeData> strike_data) {
  if (!success) {
    // Failed to get strike data; abort adding strike.
    callback.Run(0);
    return;
  }
  int num_strikes = strike_data ? strike_data->num_strikes() + 1 : 1;
  StrikeData data;
  data.set_num_strikes(num_strikes);
  data.set_last_update_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  SetStrikeData(
      key, data,
      base::BindRepeating(&StrikeDatabase::OnAddStrikeComplete,
                          base::Unretained(this), callback, num_strikes, key));
}

void StrikeDatabase::OnAddStrikeComplete(StrikesCallback callback,
                                         int num_strikes,
                                         std::string key,
                                         bool success) {
  if (success) {
    if (GetPrefixFromKey(key) == kKeyPrefixForCreditCardSave) {
      base::UmaHistogramCounts1000(
          "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave", num_strikes);
    }
    callback.Run(num_strikes);
  } else {
    callback.Run(0);
  }
}

void StrikeDatabase::OnClearAllStrikes(ClearStrikesCallback callback,
                                       bool success) {
  callback.Run(success);
}

void StrikeDatabase::OnClearAllStrikesForKey(ClearStrikesCallback callback,
                                             bool success) {
  callback.Run(success);
}

void StrikeDatabase::LoadKeys(const LoadKeysCallback& callback) {
  db_->LoadKeys(callback);
}

std::string StrikeDatabase::CreateKey(const std::string& type_prefix,
                                      const std::string& identifier_suffix) {
  return type_prefix + kKeyDeliminator + identifier_suffix;
}

std::string StrikeDatabase::GetKeyPrefixForCreditCardSave() {
  return kKeyPrefixForCreditCardSave;
}

std::string StrikeDatabase::GetPrefixFromKey(const std::string& key) {
  return key.substr(0, key.find(kKeyDeliminator));
}

}  // namespace autofill
