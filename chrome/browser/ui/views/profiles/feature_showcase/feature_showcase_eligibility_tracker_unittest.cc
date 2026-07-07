// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_eligibility_tracker.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_step_eligibility_checker.h"
#include "chrome/test/base/testing_profile.h"
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
  MockFeatureShowcaseStepEligibilityChecker() {
    ON_CALL(*this, OnTimeout()).WillByDefault(Return(false));
  }

  MOCK_METHOD(void,
              CheckEligibility,
              (Profile&, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(std::string, GetStepIdentifier, (), (const, override));
  MOCK_METHOD(bool, OnTimeout, (), (override));
};

class FeatureShowcaseEligibilityTrackerTest : public testing::Test {
 public:
  FeatureShowcaseEligibilityTrackerTest() = default;
  ~FeatureShowcaseEligibilityTrackerTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};
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

TEST_F(FeatureShowcaseEligibilityTrackerTest, SomeStepsTimeout) {
  base::OnceCallback<void(bool)> callback_1;
  auto checker_1 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_1, GetStepIdentifier()).WillByDefault(Return("step_1"));
  EXPECT_CALL(*checker_1, CheckEligibility)
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        callback_1 = std::move(callback);
      });

  base::OnceCallback<void(bool)> callback_2;
  auto checker_2 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_2, GetStepIdentifier()).WillByDefault(Return("step_2"));
  EXPECT_CALL(*checker_2, CheckEligibility)
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        callback_2 = std::move(callback);
      });

  base::OnceCallback<void(bool)> callback_3;
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

  ASSERT_FALSE(future.IsReady());

  // `checker_1` finishes on time, eligible.
  std::move(callback_1).Run(true);
  ASSERT_FALSE(future.IsReady());

  // `checker_2` finishes on time, ineligible.
  std::move(callback_2).Run(false);
  ASSERT_FALSE(future.IsReady());

  // Time out.
  task_environment_.FastForwardBy(base::Seconds(2));

  // `FeatureShowcaseEligibilityTracker` should finish evaluation on timeout.
  ASSERT_THAT(future.Get(), ElementsAre("step_1"));
}

TEST_F(FeatureShowcaseEligibilityTrackerTest, AllStepsTimeout) {
  base::OnceCallback<void(bool)> callback_1;
  auto checker_1 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_1, GetStepIdentifier()).WillByDefault(Return("step_1"));
  EXPECT_CALL(*checker_1, CheckEligibility)
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        callback_1 = std::move(callback);
      });

  base::OnceCallback<void(bool)> callback_2;
  auto checker_2 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_2, GetStepIdentifier()).WillByDefault(Return("step_2"));
  EXPECT_CALL(*checker_2, CheckEligibility)
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        callback_2 = std::move(callback);
      });

  std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>> checkers;
  checkers.push_back(std::move(checker_1));
  checkers.push_back(std::move(checker_2));

  FeatureShowcaseEligibilityTracker tracker(std::move(checkers));

  base::test::TestFuture<const std::vector<std::string>&> future;
  tracker.EvaluateEligibleSteps(profile_, future.GetCallback());

  ASSERT_FALSE(future.IsReady());

  // Fast forward past the timeout.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(FeatureShowcaseEligibilityTrackerTest,
       EvaluateEligibleStepsCancelsPreviousEvaluation) {
  base::OnceCallback<void(bool)> first_eval_callback_1;
  base::OnceCallback<void(bool)> second_eval_callback_1;
  auto checker_1 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_1, GetStepIdentifier()).WillByDefault(Return("step_1"));
  EXPECT_CALL(*checker_1, CheckEligibility)
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        first_eval_callback_1 = std::move(callback);
      })
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        second_eval_callback_1 = std::move(callback);
      });

  base::OnceCallback<void(bool)> first_eval_callback_2;
  base::OnceCallback<void(bool)> second_eval_callback_2;
  auto checker_2 =
      std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker_2, GetStepIdentifier()).WillByDefault(Return("step_2"));
  EXPECT_CALL(*checker_2, CheckEligibility)
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        first_eval_callback_2 = std::move(callback);
      })
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        second_eval_callback_2 = std::move(callback);
      });

  std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>> checkers;
  checkers.push_back(std::move(checker_1));
  checkers.push_back(std::move(checker_2));

  FeatureShowcaseEligibilityTracker tracker(std::move(checkers));

  base::test::TestFuture<const std::vector<std::string>&> first_eval_future;
  tracker.EvaluateEligibleSteps(profile_, first_eval_future.GetCallback());

  // `step_1` finishes on time at the first evaluation.
  std::move(first_eval_callback_1).Run(true);

  ASSERT_FALSE(first_eval_future.IsReady());

  // Start the second evaluation while the first one is still pending (`step_2`
  // hasn't finished).
  base::test::TestFuture<const std::vector<std::string>&> second_eval_future;
  tracker.EvaluateEligibleSteps(profile_, second_eval_future.GetCallback());

  // First evaluation pending callback should be cancelled, and the first
  // evaluation should resolve with an empty result.
  ASSERT_TRUE(first_eval_callback_2.IsCancelled());
  ASSERT_THAT(first_eval_future.Get(), IsEmpty());

  // Complete the second evaluation, and make sure the first evaluation doesn't
  // mess with the results.
  std::move(second_eval_callback_1).Run(false);
  std::move(second_eval_callback_2).Run(true);

  EXPECT_THAT(second_eval_future.Get(), ElementsAre("step_2"));
}

TEST_F(FeatureShowcaseEligibilityTrackerTest, OnTimeoutIncludesEligibleSteps) {
  base::OnceCallback<void(bool)> callback_1;
  auto checker = std::make_unique<MockFeatureShowcaseStepEligibilityChecker>();
  ON_CALL(*checker, GetStepIdentifier()).WillByDefault(Return("step_1"));
  EXPECT_CALL(*checker, CheckEligibility)
      .WillOnce([&](Profile& profile, base::OnceCallback<void(bool)> callback) {
        callback_1 = std::move(callback);
      });
  EXPECT_CALL(*checker, OnTimeout).WillOnce(Return(true));

  std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>> checkers;
  checkers.push_back(std::move(checker));

  FeatureShowcaseEligibilityTracker tracker(std::move(checkers));

  base::test::TestFuture<const std::vector<std::string>&> future;
  tracker.EvaluateEligibleSteps(profile_, future.GetCallback());

  ASSERT_FALSE(future.IsReady());

  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_THAT(future.Get(), ElementsAre("step_1"));
}

}  // namespace
