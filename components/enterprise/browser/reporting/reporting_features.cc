// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/reporting_features.h"

#include "base/feature_list.h"

namespace enterprise_reporting {

BASE_FEATURE(kSaasUsageReporting, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBrowserLaunchMetadataReporting,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_reporting
