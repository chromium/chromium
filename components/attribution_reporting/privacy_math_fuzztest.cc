// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/fuzz_utils.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace attribution_reporting {
namespace {

// Ensures that the fast path in `GetNumStatesCached()` returns the same result
// as the slow path.
void SingleSpecNumStatesMatchesRecursive(const MaxEventLevelReports max_reports,
                                         const int num_windows,
                                         const size_t num_types) {
  const std::vector<int> windows_per_type(/*count=*/num_types,
                                          /*value=*/num_windows);

  const auto collapsed_specs =
      SpecsFromWindowList(windows_per_type,
                          /*collapse_into_single_spec=*/true);

  const auto uncollapsed_specs =
      SpecsFromWindowList(windows_per_type,
                          /*collapse_into_single_spec=*/false);

  EXPECT_EQ(GetNumStates(collapsed_specs, max_reports),
            GetNumStates(uncollapsed_specs, max_reports));
}

FUZZ_TEST(PrivacyMathTest, SingleSpecNumStatesMatchesRecursive)
    .WithDomains(
        /*max_reports=*/AnyMaxEventLevelReports(),
        /*num_windows=*/fuzztest::InRange<int>(1, kMaxEventLevelReportWindows),
        /*num_types=*/fuzztest::InRange<size_t>(0, 32));

}  // namespace
}  // namespace attribution_reporting
