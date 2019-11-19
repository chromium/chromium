// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/refresh_throttler.h"

#include <limits>
#include <set>
#include <utility>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_service.h"

namespace feed {

namespace {

// Values correspond to ntp_snippets::RequestStatus and histograms.xml
enum class RequestStatus {
  kObsolete1 = 0,
  kQuotaGranted = 1,
  kQuotaExceeded = 2,
  kObsolete2 = 3,
  kStatusCount = 4
};

// When adding a new type here, extend also the "RequestThrottlerTypes"
// <histogram_suffixes> in histograms.xml with the |name| string. First value in
// the pair is the name, second is the default requests per day.
std::pair<std::string, int> GetThrottlerParams(
    UserClassifier::UserClass user_class) {
  switch (user_class) {
    case UserClassifier::UserClass::kRareSuggestionsViewer:
      return {"SuggestionFetcherRareNTPUser", 5};
    case UserClassifier::UserClass::kActiveSuggestionsViewer:
      return {"SuggestionFetcherActiveNTPUser", 20};
    case UserClassifier::UserClass::kActiveSuggestionsConsumer:
      return {"SuggestionFetcherActiveSuggestionsConsumer", 20};
  }
}

}  // namespace

RefreshThrottler::RefreshThrottler(UserClassifier::UserClass user_class,
                                   PrefService* pref_service,
                                   base::Clock* clock)
    : pref_service_(pref_service), clock_(clock) {
  DCHECK(pref_service);
  DCHECK(clock);

  std::pair<std::string, int> throttler_params = GetThrottlerParams(user_class);
  name_ = throttler_params.first;
  max_requests_per_day_ = base::GetFieldTrialParamByFeatureAsInt(
      kInterestFeedContentSuggestions,
      base::StringPrintf("quota_%s", name_.c_str()), throttler_params.second);

  // Since the histogram names are dynamic, we cannot use the standard macros
  // and we need to lookup the histograms, instead.
  int status_count = static_cast<int>(RequestStatus::kStatusCount);
  // Corresponds to UMA_HISTOGRAM_ENUMERATION(name, sample, |status_count|).
  histogram_request_status_ = base::LinearHistogram::FactoryGet(
      base::StringPrintf("NewTabPage.RequestThrottler.RequestStatus_%s",
                         name_.c_str()),
      1, status_count, status_count + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  // Corresponds to UMA_HISTOGRAM_COUNTS_100(name, sample).
  histogram_per_day_ = base::Histogram::FactoryGet(
      base::StringPrintf("NewTabPage.RequestThrottler.PerDay_%s",
                         name_.c_str()),
      1, 100, 50, base::HistogramBase::kUmaTargetedHistogramFlag);
}

bool RefreshThrottler::RequestQuota() {
  ResetCounterIfDayChanged();

  // Increment |new_count| in a overflow safe fashion.
  int new_count = GetCount();
  if (new_count < std::numeric_limits<int>::max()) {
    new_count++;
  }
  SetCount(new_count);
  bool available = (new_count <= GetQuota());

  histogram_request_status_->Add(
      static_cast<int>(available ? RequestStatus::kQuotaGranted
                                 : RequestStatus::kQuotaExceeded));

  return available;
}

void RefreshThrottler::ResetCounterIfDayChanged() {
  // Grant new quota on local midnight to spread out when clients that start
  // making un-throttled requests to server.
  int now_day = clock_->Now().LocalMidnight().since_origin().InDays();

  if (!HasDay()) {
    // The counter is used for the first time in this profile.
    SetDay(now_day);
  } else if (now_day != GetDay()) {
    // Day has changed - report the number of requests from the previous day.
    histogram_per_day_->Add(GetCount());
    // Reset the counters.
    SetCount(0);
    SetDay(now_day);
  }
}

int RefreshThrottler::GetQuota() const {
  return max_requests_per_day_;
}

int RefreshThrottler::GetCount() const {
  return pref_service_->GetInteger(prefs::kThrottlerRequestCount);
}

void RefreshThrottler::SetCount(int count) {
  pref_service_->SetInteger(prefs::kThrottlerRequestCount, count);
}

int RefreshThrottler::GetDay() const {
  return pref_service_->GetInteger(prefs::kThrottlerRequestsDay);
}

void RefreshThrottler::SetDay(int day) {
  pref_service_->SetInteger(prefs::kThrottlerRequestsDay, day);
}

bool RefreshThrottler::HasDay() const {
  return pref_service_->HasPrefPath(prefs::kThrottlerRequestsDay);
}

}  // namespace feed
