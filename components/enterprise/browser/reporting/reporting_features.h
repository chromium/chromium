// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORTING_FEATURES_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORTING_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_reporting {

// Controls whether SaaS usage reporting is enabled.
BASE_DECLARE_FEATURE(kSaasUsageReporting);

// Controls whether Gemini in Chrome usage is reported in the SaaS usage
// report.
BASE_DECLARE_FEATURE(kGeminiInChromeUsageReporting);

// Controls whether the browser should report launch-related metadata,
// such as the exact command line switches used at startup.
BASE_DECLARE_FEATURE(kBrowserLaunchMetadataReporting);

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORTING_FEATURES_H_
