// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/early_boot_enabled_state_provider.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations::cros_early_boot::evaluate_seed {

namespace {

TEST(EarlyBootEnabledStateProviderTest, Consent) {
  EarlyBootEnabledStateProvider provider;
  EXPECT_FALSE(provider.IsConsentGiven());
}

TEST(EarlyBootEnabledStateProviderTest, ReportingEnabled) {
  EarlyBootEnabledStateProvider provider;
  EXPECT_FALSE(provider.IsReportingEnabled());
}

}  // namespace
}  // namespace variations::cros_early_boot::evaluate_seed
