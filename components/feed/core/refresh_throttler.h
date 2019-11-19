// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_REFRESH_THROTTLER_H_
#define COMPONENTS_FEED_CORE_REFRESH_THROTTLER_H_

#include <string>

#include "base/macros.h"
#include "components/feed/core/user_classifier.h"

class PrefService;

namespace base {
class Clock;
class HistogramBase;
}  // namespace base

namespace feed {

// Counts requests to perform refreshes, compares them to a daily quota, and
// reports them to UMA. In the application code, create one local instance for
// each given throttler name, identified by the UserClass. Reports to the same
// histograms that previous NTP implementation used:
//  - "NewTabPage.RequestThrottler.RequestStatus_|name|" - status of each
//  request;
//  - "NewTabPage.RequestThrottler.PerDay_|name|" - the daily count of requests.
class RefreshThrottler {
 public:
  RefreshThrottler(UserClassifier::UserClass user_class,
                   PrefService* pref_service,
                   base::Clock* clock);

  // Returns whether quota is available for another request, persists the usage
  // of said quota, and reports this information to UMA.
  bool RequestQuota();

 private:
  // Also emits the PerDay histogram if the day changed.
  void ResetCounterIfDayChanged();

  int GetQuota() const;
  int GetCount() const;
  void SetCount(int count);
  int GetDay() const;
  void SetDay(int day);
  bool HasDay() const;

  // Provides durable storage.
  PrefService* pref_service_;

  // Used to access current time, injected for testing.
  base::Clock* clock_;

  // The name used by this throttler, based off UserClass, which will be used as
  // a suffix when constructing histogram or finch param names.
  std::string name_;

  // The total requests allowed before RequestQuota() starts returning false,
  // reset every time |clock_| changes days. Read from a variation param during
  // initialization.
  int max_requests_per_day_;

  // The histograms for reporting the requests of the given |type_|.
  base::HistogramBase* histogram_request_status_;
  base::HistogramBase* histogram_per_day_;

  DISALLOW_COPY_AND_ASSIGN(RefreshThrottler);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_REFRESH_THROTTLER_H_
