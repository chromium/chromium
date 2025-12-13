// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/strike_database/strike_database_integrator_base.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/strike_database/strike_data.pb.h"
#include "components/strike_database/strike_database_base.h"
#include "components/strike_database/strike_database_features.h"

namespace strike_database {

StrikeDatabaseIntegratorBase::StrikeDatabaseIntegratorBase(
    StrikeDatabaseBase* strike_database)
    : strike_database_(strike_database) {}

StrikeDatabaseIntegratorBase::~StrikeDatabaseIntegratorBase() = default;

StrikeDatabaseIntegratorBase::StrikeDatabaseDecision
StrikeDatabaseIntegratorBase::GetStrikeDatabaseDecision(
    std::string_view id) const {
  CheckIdUniqueness(id);

  if (base::FeatureList::IsEnabled(features::kDisableStrikeSystem)) {
    // Debug/test user has disabled the strike database.
    return StrikeDatabaseDecision::kDoNotBlock;
  }

  // Returns whether or not strike count for `id` has reached the strike limit
  // set by GetMaxStrikesLimit().
  if (GetStrikes(id) >= GetMaxStrikesLimit()) {
    return StrikeDatabaseDecision::kMaxStrikeLimitReached;
  }

  // Returns whether or not `GetRequiredDelaySinceLastStrike()` has passed since
  // the last strike was logged for candidate with `id`. Note that some features
  // don't specify a required delay.
  if (GetRequiredDelaySinceLastStrike().has_value() &&
      base::Time::Now() -
              strike_database_->GetLastUpdatedTimestamp(GetKey(id)) <
          GetRequiredDelaySinceLastStrike()) {
    return StrikeDatabaseDecision::kRequiredDelayNotPassed;
  }

  return StrikeDatabaseDecision::kDoNotBlock;
}

StrikeDatabaseIntegratorBase::StrikeDatabaseDecision
StrikeDatabaseIntegratorBase::GetStrikeDatabaseDecision() const {
  return GetStrikeDatabaseDecision(kSharedId);
}

bool StrikeDatabaseIntegratorBase::ShouldBlockFeature(
    std::string_view id) const {
  return GetStrikeDatabaseDecision(id) != StrikeDatabaseDecision::kDoNotBlock;
}

bool StrikeDatabaseIntegratorBase::ShouldBlockFeature() const {
  return GetStrikeDatabaseDecision() != StrikeDatabaseDecision::kDoNotBlock;
}

int StrikeDatabaseIntegratorBase::AddStrike(std::string_view id) {
  CheckIdUniqueness(id);
  return AddStrikes(1, id);
}

int StrikeDatabaseIntegratorBase::AddStrikes(int strikes_increase,
                                             std::string_view id) {
  CheckIdUniqueness(id);
  int num_strikes = strike_database_->AddStrikes(strikes_increase, GetKey(id));
  // If a new strike entry was created, run the routine to limit the number of
  // stored entries. This is a noop for most strike counters.
  if (num_strikes == strikes_increase) {
    LimitNumberOfStoredEntries();
  }

  base::UmaHistogramCounts1000(
      "Autofill.StrikeDatabase.NthStrikeAdded." + GetProjectPrefix(),
      num_strikes);
  return num_strikes;
}

int StrikeDatabaseIntegratorBase::RemoveStrike(std::string_view id) {
  CheckIdUniqueness(id);
  return strike_database_->RemoveStrikes(1, GetKey(id));
}

int StrikeDatabaseIntegratorBase::RemoveStrikes(int strike_decrease,
                                                std::string_view id) {
  CheckIdUniqueness(id);
  return strike_database_->RemoveStrikes(strike_decrease, GetKey(id));
}

int StrikeDatabaseIntegratorBase::GetStrikes(std::string_view id) const {
  CheckIdUniqueness(id);
  return strike_database_->GetStrikes(GetKey(id));
}

void StrikeDatabaseIntegratorBase::ClearStrikes(std::string_view id) {
  CheckIdUniqueness(id);
  strike_database_->ClearStrikes(GetKey(id));
}

void StrikeDatabaseIntegratorBase::ClearAllStrikes() {
  strike_database_->ClearAllStrikesForProject(GetProjectPrefix());
}

size_t StrikeDatabaseIntegratorBase::CountEntries() const {
  return std::ranges::count_if(GetStrikeCache(), [&](const auto& entry) {
    return strike_database_->GetPrefixFromKey(entry.first) ==
           GetProjectPrefix();
  });
}

void StrikeDatabaseIntegratorBase::LimitNumberOfStoredEntries() {
  if (!NumberOfEntriesExceedsLimits()) {
    return;
  }

  DCHECK(GetMaximumEntries().has_value());
  DCHECK(!GetMaximumEntriesAfterCleanup().has_value() ||
         GetMaximumEntriesAfterCleanup() <= GetMaximumEntries());

  size_t maximum_size = GetMaximumEntriesAfterCleanup().has_value()
                            ? GetMaximumEntriesAfterCleanup().value()
                            : GetMaximumEntries().value();

  std::vector<std::pair<std::string, int64_t>> entries;
  entries.reserve(GetStrikeCache().size());
  for (const auto& [key, data] : GetStrikeCache()) {
    if (strike_database_->GetPrefixFromKey(key) != GetProjectPrefix()) {
      continue;
    }
    entries.emplace_back(key, data.last_update_timestamp());
  }

  if (entries.size() <= maximum_size) {
    return;
  }

  // Sort by timestamp.
  std::ranges::sort(entries,
                    [](auto& a, auto& b) { return a.second < b.second; });
  const size_t elements_to_delete = entries.size() - maximum_size;
  const std::vector<std::string> keys_to_delete =
      base::ToVector(base::span(entries).first(elements_to_delete),
                     &std::pair<std::string, int64_t>::first);
  ClearStrikesForKeys(keys_to_delete);
}

bool StrikeDatabaseIntegratorBase::NumberOfEntriesExceedsLimits() const {
  if (!GetMaximumEntries().has_value()) {
    return false;
  }

  return CountEntries() > GetMaximumEntries();
}

void StrikeDatabaseIntegratorBase::RemoveExpiredStrikes() {
  if (!GetExpiryTimeDelta().has_value()) {
    // Strikes don't expire.
    return;
  }
  std::vector<std::string> expired_keys;
  for (const auto& [key, data] : strike_database_->GetStrikeCache()) {
    // Only consider keys from the current strike database integrator.
    if (strike_database_->GetPrefixFromKey(key) != GetProjectPrefix()) {
      continue;
    }
    if (GetEntryAge(data) > GetExpiryTimeDelta().value()) {
      if (strike_database_->GetStrikes(key) > 0) {
        expired_keys.push_back(key);
        base::UmaHistogramCounts1000(
            "Autofill.StrikeDatabase.StrikesPresentWhenStrikeExpired." +
                strike_database_->GetPrefixFromKey(key),
            strike_database_->GetStrikes(key));
      }
    }
  }
  for (std::string key : expired_keys) {
    int strikes_to_remove = 1;
    // If the key is already over the limit, remove additional strikes to
    // emulate setting it back to the limit. These are done together to avoid
    // multiple calls to the file system ProtoDatabase.
    strikes_to_remove +=
        std::max(0, strike_database_->GetStrikes(key) - GetMaxStrikesLimit());
    strike_database_->RemoveStrikes(strikes_to_remove, key);
  }
}

void StrikeDatabaseIntegratorBase::ClearStrikesByIdMatching(
    const std::set<std::string>& ids_to_delete,
    base::FunctionRef<std::string(const std::string&)> id_map) {
  ClearStrikesByIdMatchingAndTime(ids_to_delete, base::Time::Min(),
                                  base::Time::Max(), id_map);
}

void StrikeDatabaseIntegratorBase::ClearStrikesByIdMatchingAndTime(
    const std::set<std::string>& ids_to_delete,
    base::Time delete_begin,
    base::Time delete_end,
    base::FunctionRef<std::string(const std::string&)> id_map) {
  if (delete_begin.is_null()) {
    delete_begin = base::Time::Min();
  }

  if (delete_end.is_null()) {
    delete_end = base::Time::Max();
  }

  std::vector<std::string> keys_to_delete;
  keys_to_delete.reserve(GetStrikeCache().size());

  for (auto const& [key, strike_data] : GetStrikeCache()) {
    const std::string_view strike_id = GetIdFromKey(key);
    if (strike_id.empty()) {
      continue;
    }

    base::Time last_update = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(strike_data.last_update_timestamp()));

    // Check if the time stamp of the record is within deletion range and if the
    // domain is deleted.
    if (last_update >= delete_begin && last_update <= delete_end &&
        ids_to_delete.contains(id_map(std::string(strike_id)))) {
      keys_to_delete.push_back(key);
    }
  }

  ClearStrikesForKeys(keys_to_delete);
}

void StrikeDatabaseIntegratorBase::ClearStrikesForKeys(
    const std::vector<std::string>& keys) {
  strike_database_->ClearStrikesForKeys(keys);
}

std::string_view StrikeDatabaseIntegratorBase::GetIdFromKey(
    std::string_view key) const {
  std::string prefix =
      base::StrCat({GetProjectPrefix(), StrikeDatabaseBase::kKeyDeliminator});
  if (!key.starts_with(prefix)) {
    return {};
  }
  return key.substr(prefix.length(), std::string::npos);
}

base::TimeDelta StrikeDatabaseIntegratorBase::GetEntryAge(
    const StrikeData& strike_data) {
  return base::Time::Now() -
         base::Time::FromDeltaSinceWindowsEpoch(
             base::Microseconds(strike_data.last_update_timestamp()));
}

std::string StrikeDatabaseIntegratorBase::GetKey(std::string_view id) const {
  return base::StrCat(
      {GetProjectPrefix(), StrikeDatabaseBase::kKeyDeliminator, id});
}

std::optional<size_t> StrikeDatabaseIntegratorBase::GetMaximumEntries() const {
  return std::nullopt;
}

std::optional<size_t>
StrikeDatabaseIntegratorBase::GetMaximumEntriesAfterCleanup() const {
  return std::nullopt;
}

std::optional<base::TimeDelta>
StrikeDatabaseIntegratorBase::GetRequiredDelaySinceLastStrike() const {
  return std::nullopt;
}

}  // namespace strike_database
