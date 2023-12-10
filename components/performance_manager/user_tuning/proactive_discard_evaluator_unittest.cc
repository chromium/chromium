// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/user_tuning/proactive_discard_evaluator.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/public/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class TestSampler : public ProactiveDiscardEvaluator::Sampler {
 public:
  ~TestSampler() override = default;

  void TriggerSample() {
    Sample(nullptr);  // Passing nullptr to the evaluator since this test
                      // exercises code paths that are independent of the tab
                      // being considered (i.e. all tab-related code is mocked).
  }
};

class MockEstimator
    : public ProactiveDiscardEvaluator::RevisitProbabilityEstimator {
 public:
  ~MockEstimator() override = default;

  MOCK_METHOD(float,
              ComputeRevisitProbability,
              (const TabPageDecorator::TabHandle* tab_handle),
              (override));
};

class ProactiveDiscardEvaluatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setting the target to 30% means the tab will be discarded if the
    // probability it is revisited before the threshold is lower than 70%.
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kProbabilisticProactiveDiscarding,
        {{"proactive_discarding_target_false_positive_percent", "30"}});

    auto estimator = std::make_unique<MockEstimator>();
    estimator_ = estimator.get();
    auto sampler = std::make_unique<TestSampler>();
    sampler_ = sampler.get();

    evaluator_ = std::make_unique<ProactiveDiscardEvaluator>(
        std::move(estimator), std::move(sampler),
        base::BindRepeating(&ProactiveDiscardEvaluatorTest::IncrementDiscard,
                            base::Unretained(this)));
  }

  void IncrementDiscard(const TabPageDecorator::TabHandle* tab_handle) {
    ++discard_count_;
  }

  // owns the sampler_ and the estimator_
  std::unique_ptr<ProactiveDiscardEvaluator> evaluator_;

  raw_ptr<MockEstimator> estimator_;
  raw_ptr<TestSampler> sampler_;

  int discard_count_ = 0;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ProactiveDiscardEvaluatorTest, DiscardsIfUnlikelyToRevisit) {
  // Mock returning a probability of revisit of 20%, which is lower that the
  // target of 30% set in SetUp. This should lead to the tab being discarded.
  EXPECT_CALL(*estimator_, ComputeRevisitProbability)
      .WillOnce(testing::Return(0.2f))
      .WillOnce(testing::Return(0.2f));
  EXPECT_EQ(discard_count_, 0);

  EXPECT_TRUE(evaluator_->TryDiscard(nullptr));
  EXPECT_EQ(discard_count_, 1);

  sampler_->TriggerSample();
  EXPECT_EQ(discard_count_, 2);
}

TEST_F(ProactiveDiscardEvaluatorTest, DoesntDiscardIfLikelyToRevisit) {
  // Mock returning a probability of 40%, which is higher than the target of 30%
  // set in SetUp. Because the tab is more likely to be revisited than the
  // target, it won't be discarded.
  EXPECT_CALL(*estimator_, ComputeRevisitProbability)
      .WillOnce(testing::Return(0.4f))
      .WillOnce(testing::Return(0.4f));
  EXPECT_EQ(discard_count_, 0);

  EXPECT_FALSE(evaluator_->TryDiscard(nullptr));
  EXPECT_EQ(discard_count_, 0);

  sampler_->TriggerSample();
  EXPECT_EQ(discard_count_, 0);
}

}  // namespace performance_manager
