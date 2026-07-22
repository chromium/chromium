// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/gemini_step_eligibility_checker.h"

#include <memory>

#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class GeminiStepEligibilityCheckerTest : public testing::Test {
 public:
  GeminiStepEligibilityCheckerTest() = default;

  Profile& profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(GeminiStepEligibilityCheckerTest, AlwaysIneligible) {
  GeminiStepEligibilityChecker checker;
  base::test::TestFuture<bool> future;
  checker.CheckEligibility(profile(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}
