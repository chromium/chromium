// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_FEATURES_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_FEATURES_H_

#include "base/time/time.h"

namespace base {
struct Feature;
}

namespace search_features {

// Enables a burn-in period for query search. Display of results in query
// search are delayed until the burn-in period has elapsed.
// Only valid when categorical search is enabled.
extern const base::Feature kQuerySearchBurnInPeriod;

bool IsQuerySearchBurnInPeriodEnabled();

// Returns the duration of the burn-in period, in milliseconds. If parameter
// cannot be retrieved, the default value is 100ms.
base::TimeDelta QuerySearchBurnInPeriodDuration();

}  // namespace search_features

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_FEATURES_H_
