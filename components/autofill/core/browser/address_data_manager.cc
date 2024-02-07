// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_data_manager.h"

#include <memory>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/metrics/profile_token_quality_metrics.h"
#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/webdata/common/web_data_results.h"

namespace autofill {

namespace {

// Orders all `profiles` by the specified `order` rule.
void OrderProfiles(std::vector<AutofillProfile*>& profiles,
                   AddressDataManager::ProfileOrder order) {
  switch (order) {
    case AddressDataManager::ProfileOrder::kNone:
      break;
    case AddressDataManager::ProfileOrder::kHighestFrecencyDesc:
      base::ranges::sort(profiles, [comparison_time = AutofillClock::Now()](
                                       AutofillProfile* a, AutofillProfile* b) {
        return a->HasGreaterRankingThan(b, comparison_time);
      });
      break;
    case AddressDataManager::ProfileOrder::kMostRecentlyModifiedDesc:
      base::ranges::sort(profiles, [](AutofillProfile* a, AutofillProfile* b) {
        return a->modification_date() > b->modification_date();
      });
      break;
    case AddressDataManager::ProfileOrder::kMostRecentlyUsedFirstDesc:
      base::ranges::sort(profiles, [](AutofillProfile* a, AutofillProfile* b) {
        return a->use_date() > b->use_date();
      });
      break;
  }
}

}  // namespace

AddressDataManager::AddressDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service,
    const std::string& app_locale)
    : webdata_service_(webdata_service), app_locale_(app_locale) {}

AddressDataManager::~AddressDataManager() = default;

void AddressDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  CHECK(handle == pending_synced_local_profiles_query_ ||
        handle == pending_account_profiles_query_);

  if (!result) {
    // Error from the database.
    if (handle == pending_synced_local_profiles_query_) {
      pending_synced_local_profiles_query_ = 0;
    } else {
      pending_account_profiles_query_ = 0;
    }
  } else {
    CHECK_EQ(result->GetType(), AUTOFILL_PROFILES_RESULT);
    std::vector<std::unique_ptr<AutofillProfile>> profiles_from_db =
        static_cast<WDResult<std::vector<std::unique_ptr<AutofillProfile>>>*>(
            result.get())
            ->GetValue();
    if (handle == pending_synced_local_profiles_query_) {
      synced_local_profiles_ = std::move(profiles_from_db);
      pending_synced_local_profiles_query_ = 0;
    } else {
      account_profiles_ = std::move(profiles_from_db);
      pending_account_profiles_query_ = 0;
    }
  }

  if (HasPendingQueries()) {
    return;
  }
  if (!has_initial_load_finished_) {
    has_initial_load_finished_ = true;
    LogStoredDataMetrics();
  }
  // TODO(b/322170538): Notify observers: `PDM::Refresh()` is the only
  // mechanism to read from the database. Since the DB sequence is a sequenced
  // task runner, and since address data is queried before payments data,
  // `PDM::OnWebDataServiceRequestDone()` is always called after this
  // function. This makes sure that observers are notified.
  // By notifying observers here too, more events are triggered (once when
  // address data has finished reloading and once when credit card data has
  // finished reloading). This breaks just about every test.
}

std::vector<AutofillProfile*> AddressDataManager::GetProfiles(
    ProfileOrder order) const {
  std::vector<AutofillProfile*> a = GetProfilesFromSource(
      AutofillProfile::Source::kLocalOrSyncable, ProfileOrder::kNone);
  std::vector<AutofillProfile*> b = GetProfilesFromSource(
      AutofillProfile::Source::kAccount, ProfileOrder::kNone);
  a.reserve(a.size() + b.size());
  base::ranges::move(b, std::back_inserter(a));
  OrderProfiles(a, order);
  return a;
}

std::vector<AutofillProfile*> AddressDataManager::GetProfilesFromSource(
    AutofillProfile::Source profile_source,
    ProfileOrder order) const {
  const std::vector<std::unique_ptr<AutofillProfile>>& profiles =
      GetProfileStorage(profile_source);
  std::vector<AutofillProfile*> result;
  result.reserve(profiles.size());
  for (const std::unique_ptr<AutofillProfile>& profile : profiles) {
    result.push_back(profile.get());
  }
  OrderProfiles(result, order);
  return result;
}

AutofillProfile* AddressDataManager::GetProfileByGUID(
    const std::string& guid) const {
  std::vector<AutofillProfile*> profiles = GetProfiles();
  auto it = base::ranges::find(
      profiles, guid,
      [](const AutofillProfile* profile) { return profile->guid(); });
  return it != profiles.end() ? *it : nullptr;
}

void AddressDataManager::LoadProfiles() {
  if (!webdata_service_) {
    return;
  }

  CancelPendingQuery(pending_synced_local_profiles_query_);
  CancelPendingQuery(pending_account_profiles_query_);
  pending_synced_local_profiles_query_ = webdata_service_->GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, this);
  pending_account_profiles_query_ = webdata_service_->GetAutofillProfiles(
      AutofillProfile::Source::kAccount, this);
}

void AddressDataManager::CancelPendingQuery(
    WebDataServiceBase::Handle& handle) {
  if (!webdata_service_ || !handle) {
    return;
  }
  webdata_service_->CancelRequest(handle);
  handle = 0;
}

const std::vector<std::unique_ptr<AutofillProfile>>&
AddressDataManager::GetProfileStorage(AutofillProfile::Source source) const {
  switch (source) {
    case AutofillProfile::Source::kLocalOrSyncable:
      return synced_local_profiles_;
    case AutofillProfile::Source::kAccount:
      return account_profiles_;
  }
  NOTREACHED();
}

void AddressDataManager::LogStoredDataMetrics() const {
  const std::vector<AutofillProfile*> profiles = GetProfiles();
  autofill_metrics::LogStoredProfileMetrics(profiles);
  autofill_metrics::LogStoredProfileTokenQualityMetrics(profiles);
  autofill_metrics::LogLocalProfileSupersetMetrics(std::move(profiles),
                                                   app_locale_);
}

}  // namespace autofill
