// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_

// Utility functions for creating an App Service intent filter.

#include <string>

#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "url/gurl.h"

namespace apps_util {

// The concept of match level is taken from Android. The values are not
// necessary the same.
// See
// https://developer.android.com/reference/android/content/IntentFilter.html#constants_2
// for more details.
enum IntentFilterMatchLevel {
  kNone = 0,
  kScheme = 1,
  kHost = 2,
  kPattern = 4,
};

// Creates condition value that makes up App Service intent filter
// condition. Each condition contains a list of condition values.
// For pattern type of condition, the value match will be based on the
// |pattern_match_type| match type. If the |pattern_match_type| is kNone,
// then an exact match with the value will be required.
apps::mojom::ConditionValuePtr MakeConditionValue(
    const std::string& value,
    apps::mojom::PatternMatchType pattern_match_type);

// Creates condition that makes up App Service intent filter. Each
// intent filter contains a list of conditions with different
// condition types. Each condition contains a list of |condition_values|.
// For one condition, if the value matches one of the |condition_values|,
// then this condition is matched.
apps::mojom::ConditionPtr MakeCondition(
    apps::mojom::ConditionType condition_type,
    std::vector<apps::mojom::ConditionValuePtr> condition_values);

// Create intent filter for URL scope, with prefix matching only for the path.
// e.g. filter created for https://www.google.com/ will match any URL that
// started with https://www.google.com/*.
apps::mojom::IntentFilterPtr CreateIntentFilterForUrlScope(const GURL& url);

// Get the |intent_filter| match level. The higher the return value, the better
// the match is. For example, an filter with scheme, host and path is better
// match compare with filter with only scheme. Each condition type has a
// matching level value, and this function will return the sum of the matching
// level values of all existing condition types.
int GetFilterMatchLevel(const apps::mojom::IntentFilterPtr& intent_filter);

}  // namespace apps_util

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_
