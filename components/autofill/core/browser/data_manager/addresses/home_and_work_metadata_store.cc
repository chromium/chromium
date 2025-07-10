// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/home_and_work_metadata_store.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/usage_history_information.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"

namespace autofill {

namespace {

// Metadata is stored as a dict pref with the following keys:
constexpr std::string_view kUseCount = "use_count";
constexpr std::string_view kLastUseDate = "last_use_date";
constexpr std::string_view kRemovalModificationDate =
    "removal_modification_date";

// Returns the pref name where metadata for the Home and Work addresses are
// stored. Note that there can be at most one Home and Work address each.
std::string_view GetPrefName(AutofillProfile::RecordType record_type) {
  switch (record_type) {
    case AutofillProfile::RecordType::kAccountHome:
      return prefs::kAutofillHomeMetadata;
    case AutofillProfile::RecordType::kAccountWork:
      return prefs::kAutofillWorkMetadata;
    case AutofillProfile::RecordType::kLocalOrSyncable:
    case AutofillProfile::RecordType::kAccount:
      NOTREACHED();
  }
}

}  // namespace

HomeAndWorkMetadataStore::HomeAndWorkMetadataStore(
    PrefService* pref_service,
    base::RepeatingClosure on_change)
    : pref_service_(pref_service) {
  change_registrar_.Init(pref_service_);
  change_registrar_.Add(prefs::kAutofillHomeMetadata, on_change);
  change_registrar_.Add(prefs::kAutofillWorkMetadata, on_change);
}

std::vector<AutofillProfile> HomeAndWorkMetadataStore::ApplyMetadata(
    std::vector<AutofillProfile> profiles) {
  size_t max_use_count = 0;
  for (const AutofillProfile& profile : profiles) {
    max_use_count =
        std::max(max_use_count, profile.usage_history().use_count());
  }
  std::vector<AutofillProfile> result;
  result.reserve(profiles.size());
  for (AutofillProfile& profile : profiles) {
    if (!profile.IsHomeAndWorkProfile()) {
      result.push_back(std::move(profile));
      continue;
    }
    if (std::optional<AutofillProfile> modified_profile =
            ApplyMetadata(std::move(profile), max_use_count)) {
      result.push_back(std::move(*modified_profile));
    }
  }
  return result;
}

std::optional<AutofillProfile> HomeAndWorkMetadataStore::ApplyMetadata(
    AutofillProfile profile,
    int max_use_count) {
  const std::string_view pref_name = GetPrefName(profile.record_type());
  if (pref_service_->GetDict(pref_name).empty()) {
    base::DictValue defaults;
    // Like all new address, set the last use date to the current time.
    defaults.Set(kLastUseDate, base::TimeToValue(base::Time::Now()));
    // Boost the use count to rank Home > Work > other addresses.
    defaults.Set(kUseCount,
                 max_use_count + (profile.record_type() ==
                                  AutofillProfile::RecordType::kAccountHome));
    pref_service_->SetDict(pref_name, std::move(defaults));
  }
  const base::DictValue& metadata = pref_service_->GetDict(pref_name);
  UsageHistoryInformation& usage = profile.usage_history();
  const base::Value* removal_date = metadata.Find(kRemovalModificationDate);
  if (removal_date &&
      usage.modification_date() <= base::ValueToTime(*removal_date)) {
    return std::nullopt;
  }
  if (std::optional<int> use_count = metadata.FindInt(kUseCount)) {
    usage.set_use_count(*use_count);
  }
  const base::Value* last_use_date = metadata.Find(kLastUseDate);
  if (last_use_date && base::ValueToTime(last_use_date)) {
    usage.set_use_date(*base::ValueToTime(last_use_date));
  }
  return profile;
}

void HomeAndWorkMetadataStore::ApplyChange(
    const AutofillProfileChange& change) {
  const AutofillProfile& profile = change.data_model();
  if (!profile.IsHomeAndWorkProfile()) {
    return;
  }
  const UsageHistoryInformation& usage = profile.usage_history();
  base::DictValue metadata;
  switch (change.type()) {
    case AutofillProfileChange::ADD:
    case AutofillProfileChange::UPDATE:
      metadata.Set(kUseCount, static_cast<int>(usage.use_count()));
      metadata.Set(kLastUseDate, base::TimeToValue(usage.use_date()));
      break;
    case AutofillProfileChange::HIDE_IN_AUTOFILL:
      metadata.Set(kRemovalModificationDate,
                   base::TimeToValue(usage.modification_date()));
      break;
    case AutofillProfileChange::REMOVE:
      NOTREACHED();
  }
  pref_service_->SetDict(GetPrefName(profile.record_type()),
                         std::move(metadata));
}

}  // namespace autofill
