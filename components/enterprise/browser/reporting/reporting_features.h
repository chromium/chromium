// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORTING_FEATURES_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORTING_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace enterprise_reporting {

// Controls whether SaaS usage reporting is enabled.
BASE_DECLARE_FEATURE(kSaasUsageReporting);

// Controls whether Gemini in Chrome usage is reported in the SaaS usage
// report.
BASE_DECLARE_FEATURE(kGeminiInChromeUsageReporting);

// Controls whether the browser should report launch-related metadata,
// such as the exact command line switches used at startup.
BASE_DECLARE_FEATURE(kBrowserLaunchMetadataReporting);

// Controls whether the browser on Android should use its package name as the
// executable path identifier in CBCM reports.
BASE_DECLARE_FEATURE(kCbcmAndroidPackageNameIdentifier);

#if BUILDFLAG(IS_IOS)
// Controls whether enterprise signal sharing is enabled on iOS.
BASE_DECLARE_FEATURE(kIOSSignalSharingEnabled);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORTING_FEATURES_H_
