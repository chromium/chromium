// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_eligibility_service_impl.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/notebooks/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {

class NotebooksEligibilityServiceImplTest : public testing::Test {
 public:
  NotebooksEligibilityServiceImplTest() = default;
  ~NotebooksEligibilityServiceImplTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(NotebooksEligibilityServiceImplTest, EligibleWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kNotebooks);
  NotebooksEligibilityServiceImpl service(/*is_profile_eligible=*/true);
  EXPECT_TRUE(service.IsEligible());
  EXPECT_FALSE(service.IsEligibilityLoading());
}

TEST_F(NotebooksEligibilityServiceImplTest, IneligibleWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kNotebooks);
  NotebooksEligibilityServiceImpl service(/*is_profile_eligible=*/true);
  EXPECT_FALSE(service.IsEligible());
}

TEST_F(NotebooksEligibilityServiceImplTest, IneligibleWhenProfileIneligible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kNotebooks);
  NotebooksEligibilityServiceImpl service(/*is_profile_eligible=*/false);
  EXPECT_FALSE(service.IsEligible());
}

}  // namespace notebooks
