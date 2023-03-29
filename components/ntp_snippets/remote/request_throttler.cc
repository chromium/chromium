// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/request_throttler.h"

#include <climits>
#include <set>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ntp_snippets {

namespace {

// Quota value to use if no quota should be applied (by default).
const int kUnlimitedQuota = INT_MAX;

}  // namespace

struct RequestThrottler::RequestTypeInfo {
  const char* name;
  const char* count_pref;
  const char* interactive_count_pref;
  const char* day_pref;
  const int default_quota;
  const int default_interactive_quota;
};

const RequestThrottler::RequestTypeInfo RequestThrottler::kRequestTypeInfo[] = {
    // The following three types share the same prefs.
    // RequestCounter::RequestType::CONTENT_SUGGESTION_FETCHER_RARE_NTP_USER,
    {"SuggestionFetcherRareNTPUser", prefs::kSnippetFetcherRequestCount,
     prefs::kSnippetFetcherInteractiveRequestCount,
     prefs::kSnippetFetcherRequestsDay, 5, kUnlimitedQuota},
    // RequestCounter::RequestType::CONTENT_SUGGESTION_FETCHER_ACTIVE_NTP_USER,
    {"SuggestionFetcherActiveNTPUser", prefs::kSnippetFetcherRequestCount,
     prefs::kSnippetFetcherInteractiveRequestCount,
     prefs::kSnippetFetcherRequestsDay, 20, kUnlimitedQuota},
    // RequestCounter::RequestType::CONTENT_SUGGESTION_FETCHER_ACTIVE_SUGGESTIONS_CONSUMER,
    {"SuggestionFetcherActiveSuggestionsConsumer",
     prefs::kSnippetFetcherRequestCount,
     prefs::kSnippetFetcherInteractiveRequestCount,
     prefs::kSnippetFetcherRequestsDay, 20, kUnlimitedQuota},
    // RequestCounter::RequestType::CONTENT_SUGGESTION_THUMBNAIL,
    {"SuggestionThumbnailFetcher", prefs::kSnippetThumbnailsRequestCount,
     prefs::kSnippetThumbnailsInteractiveRequestCount,
     prefs::kSnippetThumbnailsRequestsDay, kUnlimitedQuota, kUnlimitedQuota}};

RequestThrottler::RequestThrottler(PrefService* pref_service, RequestType type)
    : pref_service_(pref_service),
      type_info_(kRequestTypeInfo[static_cast<int>(type)]) {
  DCHECK(pref_service);

  std::string quota = base::GetFieldTrialParamValueByFeature(
      ntp_snippets::kArticleSuggestionsFeature,
      base::StringPrintf("quota_%s", GetRequestTypeName()));
  if (!base::StringToInt(quota, &quota_)) {
    LOG_IF(WARNING, !quota.empty())
        << "Invalid variation parameter for quota for " << GetRequestTypeName();
    quota_ = type_info_->default_quota;
  }

  std::string interactive_quota = base::GetFieldTrialParamValueByFeature(
      ntp_snippets::kArticleSuggestionsFeature,
      base::StringPrintf("interactive_quota_%s", GetRequestTypeName()));
  if (!base::StringToInt(interactive_quota, &interactive_quota_)) {
    LOG_IF(WARNING, !interactive_quota.empty())
        << "Invalid variation parameter for interactive quota for "
        << GetRequestTypeName();
    interactive_quota_ = type_info_->default_interactive_quota;
  }
}

// static
void RequestThrottler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Collect all pref keys in a set to make sure we register each key exactly
  // once, even if they repeat.
  std::set<std::string> keys_to_register;
  for (const RequestTypeInfo& info : kRequestTypeInfo) {
    keys_to_register.insert(info.day_pref);
    keys_to_register.insert(info.count_pref);
    keys_to_register.insert(info.interactive_count_pref);
  }

  for (const std::string& key : keys_to_register) {
    registry->RegisterIntegerPref(key, 0);
  }
}

bool RequestThrottler::DemandQuotaForRequest(bool interactive_request) {
  ResetCounterIfDayChanged();

  int new_count = GetCount(interactive_request) + 1;
  SetCount(interactive_request, new_count);
  return new_count <= GetQuota(interactive_request);
}

void RequestThrottler::ResetCounterIfDayChanged() {
  // Get the date, "concatenated" into an int in "YYYYMMDD" format.
  base::Time::Exploded now_exploded{};
  base::Time::Now().LocalExplode(&now_exploded);
  int now_day = 10000 * now_exploded.year + 100 * now_exploded.month +
                now_exploded.day_of_month;

  if (!HasDay()) {
    // The counter is used for the first time in this profile.
    SetDay(now_day);
  } else if (now_day != GetDay()) {
    // Reset the counters.
    SetCount(/*interactive_request=*/false, 0);
    SetCount(/*interactive_request=*/true, 0);
    SetDay(now_day);
  }
}

const char* RequestThrottler::GetRequestTypeName() const {
  return type_info_->name;
}

// TODO(jkrcal): turn RequestTypeInfo into a proper class, move those methods
// onto the class and hide the members.
int RequestThrottler::GetQuota(bool interactive_request) const {
  return interactive_request ? interactive_quota_ : quota_;
}

int RequestThrottler::GetCount(bool interactive_request) const {
  return pref_service_->GetInteger(interactive_request
                                       ? type_info_->interactive_count_pref
                                       : type_info_->count_pref);
}

void RequestThrottler::SetCount(bool interactive_request, int count) {
  pref_service_->SetInteger(interactive_request
                                ? type_info_->interactive_count_pref
                                : type_info_->count_pref,
                            count);
}

int RequestThrottler::GetDay() const {
  return pref_service_->GetInteger(type_info_->day_pref);
}

void RequestThrottler::SetDay(int day) {
  pref_service_->SetInteger(type_info_->day_pref, day);
}

bool RequestThrottler::HasDay() const {
  return pref_service_->HasPrefPath(type_info_->day_pref);
}

}  // namespace ntp_snippets
