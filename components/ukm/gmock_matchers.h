// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_GMOCK_MATCHERS_H_
#define COMPONENTS_UKM_GMOCK_MATCHERS_H_

#include <cstdint>
#include <string_view>

#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ukm::testing {
// Whether the matched UkmEntry has a specific metric.
// Note: The matched entry must not be nullptr.
::testing::Matcher<const mojom::UkmEntry*> HasMetric(
    std::string_view metric_name);

// Whether the matched UkmEntry has a specific metric, and whether
// it has a specific value.
// Note: The matched entry must not be nullptr.
::testing::Matcher<const mojom::UkmEntry*> HasMetricWithValue(
    std::string_view metric_name,
    int64_t value);

}  // namespace ukm::testing

#endif  // COMPONENTS_UKM_GMOCK_MATCHERS_H_
