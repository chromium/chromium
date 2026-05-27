// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_eligibility_tracker.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_step_eligibility_checker.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;

class MockFeatureShowcaseStepEligibilityChecker
    : public FeatureShowcaseStepEligibilityChecker {
 public:
  MOCK_METHOD(void,
              CheckEligibility,
              (Profile&, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(std::string, GetStepIdentifier, (), (const, override));
};

class FeatureShowcaseEligibilityTrackerTest : public testing::Test {
 public:
  FeatureShowcaseEligibilityTrackerTest() = default;
  ~FeatureShowcaseEligibilityTrackerTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(FeatureShowcaseEligibilityTrackerTest, EmptyCheckers) {
  std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>> checkers;
  FeatureShowcaseEligibilityTracker tracker(std::move(checkers));

  base::test::TestFuture<const std::vector<std::string>&> future;
  tracker.EvaluateEligibleSteps(profile_, future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(FeatureShowcaseEligibilityTrackerTest, FilterIneligibleSteps) {
  auto checker_1 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_1, GetStepIdentifier()).WillByDefault(Return("step_1"));
  EXPECT_CALL(*checker_1, CheckEligibility)
      .WillOnce([](Profile& profile, base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  auto checker_2 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_2, GetStepIdentifier()).WillByDefault(Return("step_2"));
  EXPECT_CALL(*checker_2, CheckEligibility)
      .WillOnce([](Profile& profile, base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(false);
      });

  auto checker_3 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_3, GetStepIdentifier()).WillByDefault(Return("step_3"));
  EXPECT_CALL(*checker_3, CheckEligibility)
      .WillOnce([](Profile& profile, base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>> checkers;
  checkers.push_back(std::move(checker_1));
  checkers.push_back(std::move(checker_2));
  checkers.push_back(std::move(checker_3));

  FeatureShowcaseEligibilityTracker tracker(std::move(checkers));

  base::test::TestFuture<const std::vector<std::string>&> future;
  tracker.EvaluateEligibleSteps(profile_, future.GetCallback());

  EXPECT_THAT(future.Get(), ElementsAre("step_1", "step_3"));
}

TEST_F(FeatureShowcaseEligibilityTrackerTest, PriorityOrderPreserved) {
  base::OnceCallback<void(bool)> callback_1;
  base::OnceCallback<void(bool)> callback_2;
  base::OnceCallback<void(bool)> callback_3;

  auto checker_1 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_1, GetStepIdentifier()).WillByDefault(Return("step_1"));
  EXPECT_CALL(*checker_1, CheckEligibility)
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        callback_1 = std::move(callback);
      });

  auto checker_2 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_2, GetStepIdentifier()).WillByDefault(Return("step_2"));
  EXPECT_CALL(*checker_2, CheckEligibility)
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        callback_2 = std::move(callback);
      });

  auto checker_3 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_3, GetStepIdentifier()).WillByDefault(Return("step_3"));
  EXPECT_CALL(*checker_3, CheckEligibility)
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        callback_3 = std::move(callback);
      });

  std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>> checkers;
  checkers.push_back(std::move(checker_1));
  checkers.push_back(std::move(checker_2));
  checkers.push_back(std::move(checker_3));

  FeatureShowcaseEligibilityTracker tracker(std::move(checkers));

  base::test::TestFuture<const std::vector<std::string>&> future;
  tracker.EvaluateEligibleSteps(profile_, future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Complete them out of order (reverse).
  std::move(callback_3).Run(true);
  EXPECT_FALSE(future.IsReady());

  std::move(callback_1).Run(true);
  EXPECT_FALSE(future.IsReady());

  std::move(callback_2).Run(true);

  EXPECT_THAT(future.Get(), ElementsAre("step_1", "step_2", "step_3"));
}

TEST_F(FeatureShowcaseEligibilityTrackerTest, CapsAtMaximumSteps) {
  auto checker_1 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_1, GetStepIdentifier()).WillByDefault(Return("step_1"));
  EXPECT_CALL(*checker_1, CheckEligibility)
      .WillOnce([](Profile& profile, base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  auto checker_2 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_2, GetStepIdentifier()).WillByDefault(Return("step_2"));
  EXPECT_CALL(*checker_2, CheckEligibility)
      .WillOnce([](Profile& profile, base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  auto checker_3 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_3, GetStepIdentifier()).WillByDefault(Return("step_3"));
  EXPECT_CALL(*checker_3, CheckEligibility)
      .WillOnce([](Profile& profile, base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  auto checker_4 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_4, GetStepIdentifier()).WillByDefault(Return("step_4"));
  EXPECT_CALL(*checker_4, CheckEligibility)
      .WillOnce([](Profile& profile, base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>> checkers;
  checkers.push_back(std::move(checker_1));
  checkers.push_back(std::move(checker_2));
  checkers.push_back(std::move(checker_3));
  checkers.push_back(std::move(checker_4));

  FeatureShowcaseEligibilityTracker tracker(std::move(checkers));

  base::test::TestFuture<const std::vector<std::string>&> future;
  tracker.EvaluateEligibleSteps(profile_, future.GetCallback());

  EXPECT_THAT(future.Get(), ElementsAre("step_1", "step_2", "step_3"));
}

}  // namespace
