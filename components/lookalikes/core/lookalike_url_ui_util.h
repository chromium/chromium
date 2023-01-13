// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOOKALIKES_CORE_LOOKALIKE_URL_UI_UTIL_H_
#define COMPONENTS_LOOKALIKES_CORE_LOOKALIKE_URL_UI_UTIL_H_

#include "components/lookalikes/core/lookalike_url_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class Value;
}  // namespace base

namespace lookalikes {

// Allow easier reporting of UKM when no interstitial is shown.
void RecordUkmForLookalikeUrlBlockingPage(
    ukm::SourceId source_id,
    LookalikeUrlMatchType match_type,
    LookalikeUrlBlockingPageUserAction user_action,
    bool triggered_by_initial_url);

// Record UKM if not already reported for this page.
void ReportUkmForLookalikeUrlBlockingPageIfNeeded(
    ukm::SourceId& source_id,
    LookalikeUrlMatchType match_type,
    LookalikeUrlBlockingPageUserAction action,
    bool triggered_by_initial_url);

// Populates |load_time_data| for interstitial HTML.
void PopulateLookalikeUrlBlockingPageStrings(base::Value::Dict& load_time_data,
                                             const GURL& safe_url,
                                             const GURL& request_url);

// Values added to get shared interstitial HTML to play nice.
void PopulateStringsForSharedHTML(base::Value::Dict& load_time_data);

}  // namespace lookalikes

#endif  // COMPONENTS_LOOKALIKES_CORE_LOOKALIKE_URL_UI_UTIL_H_
