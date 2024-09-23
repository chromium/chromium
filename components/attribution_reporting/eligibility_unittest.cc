// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/eligibility.h"

#include <optional>

#include "components/attribution_reporting/registration_eligibility.mojom-shared.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::RegistrationEligibility;
using ::network::mojom::AttributionReportingEligibility;

TEST(EligibilityTest, GetRegistrationEligibility) {
  const struct {
    AttributionReportingEligibility input;
    std::optional<RegistrationEligibility> expected;
  } kTestCases[] = {
      {AttributionReportingEligibility::kUnset,
       RegistrationEligibility::kTrigger},
      {AttributionReportingEligibility::kEmpty, std::nullopt},
      {AttributionReportingEligibility::kEventSource,
       RegistrationEligibility::kSource},
      {AttributionReportingEligibility::kNavigationSource,
       RegistrationEligibility::kSource},
      {AttributionReportingEligibility::kTrigger,
       RegistrationEligibility::kTrigger},
      {AttributionReportingEligibility::kEventSourceOrTrigger,
       RegistrationEligibility::kSourceOrTrigger},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input);
    EXPECT_EQ(GetRegistrationEligibility(test_case.input), test_case.expected);
  }
}

}  // namespace
}  // namespace attribution_reporting
