// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/profile_metrics/browser_profile_type.h"
#include "base/supports_user_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeBrowserContext : public base::SupportsUserData {
 public:
  FakeBrowserContext() = default;
  ~FakeBrowserContext() override = default;
};

}  // namespace

namespace profile_metrics {

class BrowserProfileTypeUnitTest : public testing::Test {
 public:
  BrowserProfileTypeUnitTest() = default;
  ~BrowserProfileTypeUnitTest() override = default;
};

TEST_F(BrowserProfileTypeUnitTest, AssignmentAndRetrieval) {
  for (int i = 0; i <= static_cast<int>(BrowserProfileType::kMaxValue); i++) {
    BrowserProfileType pt = static_cast<BrowserProfileType>(i);

    FakeBrowserContext browser_context;

    SetBrowserProfileType(&browser_context, pt);
    EXPECT_EQ(pt, GetBrowserProfileType(&browser_context));
  }
}

}  // namespace profile_metrics
