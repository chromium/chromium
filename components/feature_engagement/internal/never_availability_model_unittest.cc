// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/never_availability_model.h"

#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

BASE_FEATURE(kAvailabilityTestFeatureFoo,
             "test_foo",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAvailabilityTestFeatureBar,
             "test_bar",
             base::FEATURE_DISABLED_BY_DEFAULT);

class NeverAvailabilityModelTest : public ::testing::Test {
 public:
  NeverAvailabilityModelTest()
      : availability_model_(std::make_unique<NeverAvailabilityModel>()) {}

  NeverAvailabilityModelTest(const NeverAvailabilityModelTest&) = delete;
  NeverAvailabilityModelTest& operator=(const NeverAvailabilityModelTest&) =
      delete;

  void OnInitializedCallback(bool success) { success_ = success; }

 protected:
  std::unique_ptr<NeverAvailabilityModel> availability_model_;
  std::optional<bool> success_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

}  // namespace

TEST_F(NeverAvailabilityModelTest, ShouldNeverHaveData) {
  EXPECT_EQ(std::nullopt,
            availability_model_->GetAvailability(kAvailabilityTestFeatureFoo));
  EXPECT_EQ(std::nullopt,
            availability_model_->GetAvailability(kAvailabilityTestFeatureBar));

  availability_model_->Initialize(
      base::BindOnce(&NeverAvailabilityModelTest::OnInitializedCallback,
                     base::Unretained(this)),
      14u);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::nullopt,
            availability_model_->GetAvailability(kAvailabilityTestFeatureFoo));
  EXPECT_EQ(std::nullopt,
            availability_model_->GetAvailability(kAvailabilityTestFeatureBar));
}

TEST_F(NeverAvailabilityModelTest, ShouldBeReadyAfterInitialization) {
  EXPECT_FALSE(availability_model_->IsReady());
  availability_model_->Initialize(
      base::BindOnce(&NeverAvailabilityModelTest::OnInitializedCallback,
                     base::Unretained(this)),
      14u);
  EXPECT_FALSE(availability_model_->IsReady());
  EXPECT_FALSE(success_.has_value());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(availability_model_->IsReady());
  ASSERT_TRUE(success_.has_value());
  EXPECT_TRUE(success_.value());
}

TEST_F(NeverAvailabilityModelTest, DestroyedBeforeInitialization) {
  EXPECT_FALSE(availability_model_->IsReady());
  // Initialize performs asynchronous tasks that are posted to the task queue.
  // However the class may be torn down before they have a chance to complete.
  availability_model_->Initialize(
      base::BindOnce(&NeverAvailabilityModelTest::OnInitializedCallback,
                     base::Unretained(this)),
      14u);
  availability_model_.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(success_.has_value());
}

}  // namespace feature_engagement
