// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/request_throttler.h"

#include <climits>
#include <set>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
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

// Enumeration listing all possible outcomes for fetch attempts. Used for UMA
// histogram, so do not change existing values. Insert new values at the end,
// and update the histogram definition.
enum class RequestStatus {
  INTERACTIVE_QUOTA_GRANTED,
  BACKGROUND_QUOTA_GRANTED,
  BACKGROUND_QUOTA_EXCEEDED,
  INTERACTIVE_QUOTA_EXCEEDED,
  REQUEST_STATUS_COUNT
};

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

// When adding a new type here, extend also the "RequestThrottlerTypes"
// <histogram_suffixes> in histograms.xml with the |name| string.
const RequestThrottler::RequestTypeInfo RequestThrottler::kRequestTypeInfo[] = {
    // The following three types share the same prefs. They differ in quota
    // values (and UMA histograms).
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

  // Since the histogram names are dynamic, we cannot use the standard macros
  // and we need to lookup the histograms, instead.
  int status_count = static_cast<int>(RequestStatus::REQUEST_STATUS_COUNT);
  // Corresponds to UMA_HISTOGRAM_ENUMERATION(name, sample, |status_count|).
  histogram_request_status_ = base::LinearHistogram::FactoryGet(
      base::StringPrintf("NewTabPage.RequestThrottler.RequestStatus_%s",
                         GetRequestTypeName()),
      1, status_count, status_count + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  // Corresponds to UMA_HISTOGRAM_COUNTS_100(name, sample).
  histogram_per_day_background_ = base::Histogram::FactoryGet(
      base::StringPrintf("NewTabPage.RequestThrottler.PerDay_%s",
                         GetRequestTypeName()),
      1, 100, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
  // Corresponds to UMA_HISTOGRAM_COUNTS_100(name, sample).
  histogram_per_day_interactive_ = base::Histogram::FactoryGet(
      base::StringPrintf("NewTabPage.RequestThrottler.PerDayInteractive_%s",
                         GetRequestTypeName()),
      1, 100, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
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
  bool available = (new_count <= GetQuota(interactive_request));

  if (interactive_request) {
    histogram_request_status_->Add(static_cast<int>(
        available ? RequestStatus::INTERACTIVE_QUOTA_GRANTED
                  : RequestStatus::INTERACTIVE_QUOTA_EXCEEDED));
  } else {
    histogram_request_status_->Add(
        static_cast<int>(available ? RequestStatus::BACKGROUND_QUOTA_GRANTED
                                   : RequestStatus::BACKGROUND_QUOTA_EXCEEDED));
  }
  return available;
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
    // Day has changed - report the number of requests from the previous day.
    histogram_per_day_background_->Add(GetCount(/*interactive_request=*/false));
    histogram_per_day_interactive_->Add(GetCount(/*interactive_request=*/true));
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
