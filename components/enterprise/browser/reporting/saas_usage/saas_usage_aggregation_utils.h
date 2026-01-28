// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_AGGREGATION_UTILS_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_AGGREGATION_UTILS_H_

#include <string_view>

#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

// These functions are used to aggregate domain reporting data. They store the
// aggregated data using PrefService.

void RecordNavigation(PrefService* pref_service,
                      std::string_view domain,
                      std::string_view encryption_protocol);

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_SAAS_USAGE_SAAS_USAGE_AGGREGATION_UTILS_H_
