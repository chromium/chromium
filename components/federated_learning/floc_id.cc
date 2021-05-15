// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_id.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/federated_learning/features/features.h"
#include "components/federated_learning/floc_constants.h"
#include "components/federated_learning/sim_hash.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"

namespace federated_learning {

// static
uint64_t FlocId::SimHashHistory(
    const std::unordered_set<std::string>& domains) {
  return SimHashStrings(domains, kMaxNumberOfBitsInFloc);
}

FlocId::FlocId()
    : finch_config_version_(kFlocIdFinchConfigVersion.Get()),
      compute_time_(base::Time::Now()) {}

FlocId::FlocId(uint64_t id,
               base::Time history_begin_time,
               base::Time history_end_time,
               uint32_t sorting_lsh_version)
    : id_(id),
      history_begin_time_(history_begin_time),
      history_end_time_(history_end_time),
      finch_config_version_(kFlocIdFinchConfigVersion.Get()),
      sorting_lsh_version_(sorting_lsh_version),
      compute_time_(base::Time::Now()) {}

FlocId::FlocId(const FlocId& id) = default;

FlocId::~FlocId() = default;

FlocId& FlocId::operator=(const FlocId& id) = default;

FlocId& FlocId::operator=(FlocId&& id) = default;

bool FlocId::IsValid() const {
  return id_.has_value();
}

bool FlocId::operator==(const FlocId& other) const {
  return id_ == other.id_ && history_begin_time_ == other.history_begin_time_ &&
         history_end_time_ == other.history_end_time_ &&
         finch_config_version_ == other.finch_config_version_ &&
         sorting_lsh_version_ == other.sorting_lsh_version_ &&
         compute_time_ == other.compute_time_;
}

bool FlocId::operator!=(const FlocId& other) const {
  return !(*this == other);
}

blink::mojom::InterestCohortPtr FlocId::ToInterestCohortForJsApi() const {
  // TODO(yaoxia): consider returning the version part even when floc is
  // invalid.

  DCHECK(id_.has_value());

  blink::mojom::InterestCohortPtr result = blink::mojom::InterestCohort::New();
  result->id = base::NumberToString(id_.value());
  result->version =
      base::StrCat({"chrome.", base::NumberToString(finch_config_version_), ".",
                    base::NumberToString(sorting_lsh_version_)});
  return result;
}

uint64_t FlocId::ToUint64() const {
  DCHECK(id_.has_value());
  return id_.value();
}

// static
void FlocId::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterUint64Pref(kFlocIdValuePrefKey, 0);
  registry->RegisterTimePref(kFlocIdHistoryBeginTimePrefKey, base::Time());
  registry->RegisterTimePref(kFlocIdHistoryEndTimePrefKey, base::Time());
  registry->RegisterUint64Pref(kFlocIdFinchConfigVersionPrefKey, 0);
  registry->RegisterUint64Pref(kFlocIdSortingLshVersionPrefKey, 0);
  registry->RegisterTimePref(kFlocIdComputeTimePrefKey, base::Time());
}

void FlocId::SaveToPrefs(PrefService* prefs) {
  if (!id_.has_value()) {
    prefs->ClearPref(kFlocIdValuePrefKey);
  } else {
    prefs->SetUint64(kFlocIdValuePrefKey, id_.value());
  }

  prefs->SetTime(kFlocIdHistoryBeginTimePrefKey, history_begin_time_);
  prefs->SetTime(kFlocIdHistoryEndTimePrefKey, history_end_time_);
  prefs->SetUint64(kFlocIdFinchConfigVersionPrefKey, finch_config_version_);
  prefs->SetUint64(kFlocIdSortingLshVersionPrefKey, sorting_lsh_version_);
  prefs->SetTime(kFlocIdComputeTimePrefKey, compute_time_);
}

void FlocId::InvalidateIdAndSaveToPrefs(PrefService* prefs) {
  id_.reset();
  prefs->ClearPref(kFlocIdValuePrefKey);
}

void FlocId::ResetComputeTimeAndSaveToPrefs(base::Time compute_time,
                                            PrefService* prefs) {
  compute_time_ = compute_time;
  prefs->SetTime(kFlocIdComputeTimePrefKey, compute_time_);
}

// static
FlocId FlocId::ReadFromPrefs(PrefService* prefs) {
  absl::optional<uint64_t> id;
  if (prefs->HasPrefPath(kFlocIdValuePrefKey))
    id = prefs->GetUint64(kFlocIdValuePrefKey);

  return FlocId(id, prefs->GetTime(kFlocIdHistoryBeginTimePrefKey),
                prefs->GetTime(kFlocIdHistoryEndTimePrefKey),
                prefs->GetUint64(kFlocIdFinchConfigVersionPrefKey),
                prefs->GetUint64(kFlocIdSortingLshVersionPrefKey),
                prefs->GetTime(kFlocIdComputeTimePrefKey));
}

FlocId::FlocId(absl::optional<uint64_t> id,
               base::Time history_begin_time,
               base::Time history_end_time,
               uint32_t finch_config_version,
               uint32_t sorting_lsh_version,
               base::Time compute_time)
    : id_(id),
      history_begin_time_(history_begin_time),
      history_end_time_(history_end_time),
      finch_config_version_(finch_config_version),
      sorting_lsh_version_(sorting_lsh_version),
      compute_time_(compute_time) {}

}  // namespace federated_learning
