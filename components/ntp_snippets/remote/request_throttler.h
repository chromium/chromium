// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REQUEST_THROTTLER_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REQUEST_THROTTLER_H_

#include <string>

#include "base/macros.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class HistogramBase;
}  // namespace base

namespace ntp_snippets {

// Counts requests to external services, compares them to a daily quota, reports
// them to UMA. In the application code, create one local instance for each type
// of requests, identified by the RequestType. The request counter is based on:
//  - daily quota from a variation param "quota_|type|" in the NTPSnippets trial
//  - pref "ntp.request_throttler.|type|.count" to store the current counter,
//  - pref "ntp.request_throttler.|type|.day" to store current day to which the
//    current counter value applies.
// Furthermore the counter reports to histograms:
//  - "NewTabPage.RequestThrottler.RequestStatus_|type|" - status of each
//  request;
//  - "NewTabPage.RequestThrottler.PerDay_|type|" - the daily count of requests.
//
// Implementation notes: When extending this class for a new RequestType, please
//  1) define in request_counter.cc in kRequestTypeInfo
//     a) the string value for your |type| and
//     b) constants for day/count prefs;
//  2) define a new RequestThrottlerTypes histogram suffix in histogram.xml
//     (with the same string value as in 1a)).
class RequestThrottler {
 public:
  // Enumeration listing all current applications of the request counter.
  enum class RequestType {
    CONTENT_SUGGESTION_FETCHER_RARE_NTP_USER,
    CONTENT_SUGGESTION_FETCHER_ACTIVE_NTP_USER,
    CONTENT_SUGGESTION_FETCHER_ACTIVE_SUGGESTIONS_CONSUMER,
    CONTENT_SUGGESTION_THUMBNAIL,
  };

  RequestThrottler(PrefService* pref_service, RequestType type);

  // Registers profile prefs for all RequestTypes. Called from browser_prefs.cc.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns whether quota is available for another request and reports this
  // information to UMA. Interactive requests should be always granted (upon
  // standard conditions) and should be only used for requests initiated by the
  // user (if it is safe to assume that all users cannot generate an amount of
  // requests we cannot handle).
  bool DemandQuotaForRequest(bool interactive_request);

 private:
  friend class RequestThrottlerTest;
  // Used internally for working with a RequestType.
  struct RequestTypeInfo;

  // The array of info entries - one per each RequestType.
  static const RequestTypeInfo kRequestTypeInfo[];

  // Also emits the PerDay histogram if the day changed.
  void ResetCounterIfDayChanged();

  const char* GetRequestTypeName() const;

  int GetQuota(bool interactive_request) const;
  int GetCount(bool interactive_request) const;
  void SetCount(bool interactive_request, int count);
  int GetDay() const;
  void SetDay(int day);
  bool HasDay() const;

  PrefService* pref_service_;
  const RequestTypeInfo& type_info_;

  // The quotas are hardcoded, but can be overridden by variation params.
  int quota_;
  int interactive_quota_;

  // The histograms for reporting the requests of the given |type_|.
  base::HistogramBase* histogram_request_status_;
  base::HistogramBase* histogram_per_day_background_;
  base::HistogramBase* histogram_per_day_interactive_;

  DISALLOW_COPY_AND_ASSIGN(RequestThrottler);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REQUEST_THROTTLER_H_
