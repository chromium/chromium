// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/features.h"

#include "base/feature_list.h"

namespace attribution_reporting {

BASE_FEATURE(kAttributionReportingNullAggregatableReports,
             "AttributionReportingNullAggregatableReports",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace attribution_reporting
