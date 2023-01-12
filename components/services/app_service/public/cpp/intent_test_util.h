// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_TEST_UTIL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_TEST_UTIL_H_

#include <string>

#include "components/services/app_service/public/cpp/intent_filter.h"

namespace apps_util {

// Matches against intent.mime_type only, and not the mime type of files in the
// intent.
apps::IntentFilterPtr MakeIntentFilterForMimeType(const std::string& mime_type);

// Creates intent filter for send action. Matches against intents that have
// files.
apps::IntentFilterPtr MakeIntentFilterForSend(
    const std::string& mime_types,
    const std::string& activity_label = "");

// Creates intent filter for send multiple action.
apps::IntentFilterPtr MakeIntentFilterForSendMultiple(
    const std::string& mime_types,
    const std::string& activity_label = "");

apps::IntentFilterPtr MakeFileFilterForView(const std::string& mime_type,
                                            const std::string& file_extension,
                                            const std::string& activity_label);

apps::IntentFilterPtr MakeURLFilterForView(const std::string& url_pattern,
                                           const std::string& activity_label);

// Creates intent filter that contains only the `scheme`.
apps::IntentFilterPtr MakeSchemeOnlyFilter(const std::string& scheme);

// Creates intent filter that contains only the `scheme` and `host`.
apps::IntentFilterPtr MakeSchemeAndHostOnlyFilter(const std::string& scheme,
                                                  const std::string& host);

// Add a condition value to the |intent_filter|. If the |condition_type|
// exists, add the condition value to the existing condition, otherwise
// create new condition.
void AddConditionValue(apps::ConditionType condition_type,
                       const std::string& value,
                       apps::PatternMatchType pattern_match_type,
                       apps::IntentFilterPtr& intent_filter);

}  // namespace apps_util

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_TEST_UTIL_H_
