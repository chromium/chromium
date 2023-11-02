// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_REQUEST_THROTTLER_H_
#define COMPONENTS_FEED_CORE_V2_REQUEST_THROTTLER_H_

#include "base/memory/raw_ptr.h"
#include "components/feed/core/v2/enums.h"

class PrefService;

namespace feed {

// Limits number of network requests that can be made each day.
class RequestThrottler {
 public:
  explicit RequestThrottler(PrefService* pref_service);

  RequestThrottler(const RequestThrottler&) = delete;
  RequestThrottler& operator=(const RequestThrottler&) = delete;

  // Returns whether quota is available for another request, persists the usage
  // of said quota, and reports this information to UMA.
  bool RequestQuota(NetworkRequestType request_type);

 private:
  void ResetCountersIfDayChanged();

  // Provides durable storage.
  raw_ptr<PrefService> pref_service_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_REQUEST_THROTTLER_H_
