// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/features.h"

namespace enterprise_connectors {

BASE_FEATURE(kEnterpriseSecurityEventReportingOnAndroid,
             "EnterpriseSecurityEventReportingOnAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnterpriseUrlFilteringEventReportingOnAndroid,
             "EnterpriseUrlFilteringEventReportingOnAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnterpriseRealtimeEventReportingOnIOS,
             "EnterpriseRealtimeEventReportingOnIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnterpriseActiveUserDetection,
             "EnterpriseActiveUserDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnterpriseIframeDlpRulesSupport,
             "EnterpriseIframeDlpRulesSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_connectors
