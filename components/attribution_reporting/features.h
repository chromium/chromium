// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_FEATURES_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace attribution_reporting {

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
BASE_DECLARE_FEATURE(kAttributionReportingNullAggregatableReports);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_FEATURES_H_
