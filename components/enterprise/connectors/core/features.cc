// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/features.h"

namespace enterprise_connectors {

BASE_FEATURE(kEnterpriseSecurityEventReportingOnAndroid,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnterpriseActiveUserDetection, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnterpriseIframeDlpRulesSupport,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace enterprise_connectors
