// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"

#include "base/strings/to_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

TEST(TpcdExperimentEligibilityTest, Eligibility) {
  for (int i = 0;
       i < static_cast<int>(TpcdExperimentEligibility::Reason::kMaxValue);
       ++i) {
    auto reason = TpcdExperimentEligibility::Reason(i);
    auto eligibility = TpcdExperimentEligibility(reason);
    ASSERT_EQ(eligibility.reason(), reason);
    switch (reason) {
      case TpcdExperimentEligibility::Reason::kEligible:
      case TpcdExperimentEligibility::Reason::kForcedEligible:
        EXPECT_TRUE(eligibility.is_eligible())
            << "Expected Reason(" << base::ToString(reason)
            << ") to be eligible";
        break;
      case TpcdExperimentEligibility::Reason::k3pCookiesBlocked:
      case TpcdExperimentEligibility::Reason::kHasNotSeenNotice:
      case TpcdExperimentEligibility::Reason::kNewUser:
      case TpcdExperimentEligibility::Reason::kEnterpriseUser:
      case TpcdExperimentEligibility::Reason::kPwaOrTwaInstalled:
        EXPECT_FALSE(eligibility.is_eligible())
            << "Expected Reason(" << base::ToString(reason)
            << ") not to be eligible";
        break;
    }
  }
}

}  // namespace
}  // namespace privacy_sandbox
