// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_FUZZ_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_FUZZ_UTILS_H_

#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace attribution_reporting {

class MaxEventLevelReports;

fuzztest::Domain<mojom::SourceType> AnySourceType();

fuzztest::Domain<MaxEventLevelReports> AnyMaxEventLevelReports();

fuzztest::Domain<FilterData> AnyFilterData();

fuzztest::Domain<FilterConfig> AnyFilterConfig();

fuzztest::Domain<FiltersDisjunction> AnyFiltersDisjunction();

fuzztest::Domain<FilterPair> AnyFilterPair();

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_FUZZ_UTILS_H_
