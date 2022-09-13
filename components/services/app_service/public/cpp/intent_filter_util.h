// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_

// Utility functions for creating an App Service intent filter.

#include <string>

#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "url/gurl.h"

namespace apps_util {

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

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
void AddSingleValueCondition(apps::mojom::ConditionType condition_type,
                             const std::string& value,
                             apps::mojom::PatternMatchType pattern_match_type,
                             apps::mojom::IntentFilterPtr& intent_filter);

// Create intent filter for URL scope, with prefix matching only for the path.
// e.g. filter created for https://www.google.com/ will match any URL that
// started with https://www.google.com/*.

// TODO(crbug.com/1092784): Update/add all related unit tests to test with
// action view.
apps::IntentFilterPtr MakeIntentFilterForUrlScope(const GURL& url);

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
apps::mojom::IntentFilterPtr CreateIntentFilterForUrlScope(const GURL& url);

// Get the |intent_filter| match level. The higher the return value, the better
// the match is. For example, an filter with scheme, host and path is better
// match compare with filter with only scheme. Each condition type has a
// matching level value, and this function will return the sum of the matching
// level values of all existing condition types.
// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
int GetFilterMatchLevel(const apps::mojom::IntentFilterPtr& intent_filter);

// Check if the two intent filters have overlap. i.e. they can handle same
// intent with same match level.
bool FiltersHaveOverlap(const apps::IntentFilterPtr& filter1,
                        const apps::IntentFilterPtr& filter2);

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
bool FiltersHaveOverlap(const apps::mojom::IntentFilterPtr& filter1,
                        const apps::mojom::IntentFilterPtr& filter2);

// Check if the filter is the older version that doesn't contain action.
// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
bool FilterNeedsUpgrade(const apps::mojom::IntentFilterPtr& filter);

// Upgrade the filter to contain action view.
void UpgradeFilter(apps::IntentFilterPtr& filter);

// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
void UpgradeFilter(apps::mojom::IntentFilterPtr& filter);

// Check if the filter is a browser filter, i.e. can handle all https
// or http scheme.
// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
bool IsBrowserFilter(const apps::mojom::IntentFilterPtr& filter);

// Checks if the `intent_filter` is a supported link for `app_id`, i.e. it has
// the "view" action, a http or https scheme, and at least one host and pattern.
bool IsSupportedLinkForApp(const std::string& app_id,
                           const apps::IntentFilterPtr& intent_filter);

// Check if the |intent_filter| is a supported link for |app_id|, i.e. it has
// the "view" action, a http or https scheme, and at least one host and pattern.
// TODO(crbug.com/1253250): Remove after migrating to non-mojo AppService.
bool IsSupportedLinkForApp(const std::string& app_id,
                           const apps::mojom::IntentFilterPtr& intent_filter);

// If |intent_filter| matches a URL by prefix, return the length of the longest
// match. For example, if the filter matches "https://example.org/app/*", the
// longest match for the URL "https://example.org/app/foo/bar" is the length of
// "https://example.org/app/" (24). If |intent_filter| does not match |url|, or
// if the filter does not match a prefix (e.g. glob), 0 is returned.
size_t IntentFilterUrlMatchLength(const apps::IntentFilterPtr& intent_filter,
                                  const GURL& url);

}  // namespace apps_util

namespace apps {

// Pretty-prints |intent_filter| for debugging purposes.
std::ostream& operator<<(std::ostream& out,
                         const apps::mojom::IntentFilterPtr& intent_filter);
}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_FILTER_UTIL_H_
