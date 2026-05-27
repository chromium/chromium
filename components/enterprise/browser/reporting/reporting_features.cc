// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/reporting_features.h"

#include "base/feature_list.h"

namespace enterprise_reporting {

BASE_FEATURE(kSaasUsageReporting, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGeminiInChromeUsageReporting, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBrowserLaunchMetadataReporting,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCbcmAndroidPackageNameIdentifier,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kIOSSignalSharingEnabled, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace enterprise_reporting
