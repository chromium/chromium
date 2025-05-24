// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_FEATURES_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_connectors {

// Controls whether event reporting is enabled on Android
BASE_DECLARE_FEATURE(kEnterpriseSecurityEventReportingOnAndroid);

// Controls whether url filtering event reporting is enabled on Android, even if
// `kEnterpriseSecurityEventReportingOnAndroid` is not enabled.
BASE_DECLARE_FEATURE(kEnterpriseUrlFilteringEventReportingOnAndroid);

// Controls whether the realtime events reporting is enabled on iOS.
BASE_DECLARE_FEATURE(kEnterpriseRealtimeEventReportingOnIOS);

// Controls whether enterprise features will attempt to attach the active
// content area user email to DLP/reporting requests on Workspace sites.
BASE_DECLARE_FEATURE(kEnterpriseActiveUserDetection);

// Controls whether the iFrame parent url chain initiated from the active frame
// will be attached to DLP scan requests.
BASE_DECLARE_FEATURE(kEnterpriseIframeDlpRulesSupport);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_FEATURES_H_
