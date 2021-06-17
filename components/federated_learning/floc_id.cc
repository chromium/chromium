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

// static
FlocId FlocId::CreateInvalid(Status status) {
  DCHECK_NE(Status::kValid, status);
  DCHECK_NE(Status::kInvalidNoStatusPrefs, status);
  return FlocId(/*id=*/0, status,
                /*history_begin_time=*/base::Time(),
                /*history_end_time=*/base::Time(),
                kFlocIdFinchConfigVersion.Get(),
                /*sorting_lsh_version=*/0,
                /*compute_time=*/base::Time::Now());
}

// static
FlocId FlocId::CreateValid(uint64_t id,
                           base::Time history_begin_time,
                           base::Time history_end_time,
                           uint32_t sorting_lsh_version) {
  return FlocId(id, Status::kValid, history_begin_time, history_end_time,
                kFlocIdFinchConfigVersion.Get(), sorting_lsh_version,
                /*compute_time=*/base::Time::Now());
}

FlocId::FlocId(const FlocId& id) = default;

FlocId::~FlocId() = default;

FlocId& FlocId::operator=(const FlocId& id) = default;

FlocId& FlocId::operator=(FlocId&& id) = default;

bool FlocId::IsValid() const {
  return status_ == Status::kValid;
}

bool FlocId::operator==(const FlocId& other) const {
  return id_ == other.id_ && status_ == other.status_ &&
         history_begin_time_ == other.history_begin_time_ &&
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

  DCHECK_EQ(Status::kValid, status_);

  blink::mojom::InterestCohortPtr result = blink::mojom::InterestCohort::New();
  result->id = base::NumberToString(id_);
  result->version =
      base::StrCat({"chrome.", base::NumberToString(finch_config_version_), ".",
                    base::NumberToString(sorting_lsh_version_)});
  return result;
}

uint64_t FlocId::ToUint64() const {
  DCHECK_EQ(Status::kValid, status_);
  return id_;
}

// static
void FlocId::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterUint64Pref(kFlocIdValuePrefKey, 0);
  registry->RegisterIntegerPref(
      kFlocIdStatusPrefKey, static_cast<int>(Status::kInvalidNoStatusPrefs));
  registry->RegisterTimePref(kFlocIdHistoryBeginTimePrefKey, base::Time());
  registry->RegisterTimePref(kFlocIdHistoryEndTimePrefKey, base::Time());
  registry->RegisterUint64Pref(kFlocIdFinchConfigVersionPrefKey, 0);
  registry->RegisterUint64Pref(kFlocIdSortingLshVersionPrefKey, 0);
  registry->RegisterTimePref(kFlocIdComputeTimePrefKey, base::Time());
}

void FlocId::SaveToPrefs(PrefService* prefs) {
  DCHECK_NE(status_, Status::kInvalidNoStatusPrefs);

  prefs->SetUint64(kFlocIdValuePrefKey, id_);
  prefs->SetInteger(kFlocIdStatusPrefKey, static_cast<int>(status_));
  prefs->SetTime(kFlocIdHistoryBeginTimePrefKey, history_begin_time_);
  prefs->SetTime(kFlocIdHistoryEndTimePrefKey, history_end_time_);
  prefs->SetUint64(kFlocIdFinchConfigVersionPrefKey, finch_config_version_);
  prefs->SetUint64(kFlocIdSortingLshVersionPrefKey, sorting_lsh_version_);
  prefs->SetTime(kFlocIdComputeTimePrefKey, compute_time_);
}

void FlocId::UpdateStatusAndSaveToPrefs(PrefService* prefs, Status status) {
  DCHECK_NE(status, Status::kValid);
  DCHECK_NE(status, Status::kInvalidNoStatusPrefs);
  status_ = status;
  prefs->SetInteger(kFlocIdStatusPrefKey, static_cast<int>(status_));
}

void FlocId::ResetComputeTimeAndSaveToPrefs(base::Time compute_time,
                                            PrefService* prefs) {
  compute_time_ = compute_time;
  prefs->SetTime(kFlocIdComputeTimePrefKey, compute_time_);
}

// static
FlocId FlocId::ReadFromPrefs(PrefService* prefs) {
  Status status = Status::kInvalidNoStatusPrefs;

  // We rely on the time to tell whether it's a fresh profile.
  if (!prefs->GetTime(kFlocIdComputeTimePrefKey).is_null()) {
    // In some previous pref version before the status field is introduced, we
    // used to use null path to represent invalid floc. After the status field
    // is introduced, that state is represented in status.
    if (prefs->HasPrefPath(kFlocIdValuePrefKey)) {
      if (prefs->HasPrefPath(kFlocIdStatusPrefKey)) {
        status = static_cast<Status>(prefs->GetInteger(kFlocIdStatusPrefKey));
        DCHECK_NE(status, Status::kInvalidNoStatusPrefs);
      } else {
        status = Status::kValid;
      }
    }
  }

  return FlocId(prefs->GetUint64(kFlocIdValuePrefKey), status,
                prefs->GetTime(kFlocIdHistoryBeginTimePrefKey),
                prefs->GetTime(kFlocIdHistoryEndTimePrefKey),
                prefs->GetUint64(kFlocIdFinchConfigVersionPrefKey),
                prefs->GetUint64(kFlocIdSortingLshVersionPrefKey),
                prefs->GetTime(kFlocIdComputeTimePrefKey));
}

FlocId::FlocId(uint64_t id,
               Status status,
               base::Time history_begin_time,
               base::Time history_end_time,
               uint32_t finch_config_version,
               uint32_t sorting_lsh_version,
               base::Time compute_time)
    : id_(id),
      status_(status),
      history_begin_time_(history_begin_time),
      history_end_time_(history_end_time),
      finch_config_version_(finch_config_version),
      sorting_lsh_version_(sorting_lsh_version),
      compute_time_(compute_time) {}

}  // namespace federated_learning
