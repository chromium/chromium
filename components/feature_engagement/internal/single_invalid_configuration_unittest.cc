// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/single_invalid_configuration.h"

#include "base/feature_list.h"
#include "components/feature_engagement/public/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

const base::Feature kSingleTestFeatureFoo{"test_foo",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kSingleTestFeatureBar{"test_bar",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

class SingleInvalidConfigurationTest : public ::testing::Test {
 public:
  SingleInvalidConfigurationTest() = default;

 protected:
  SingleInvalidConfiguration configuration_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleInvalidConfigurationTest);
};

}  // namespace

TEST_F(SingleInvalidConfigurationTest, AllConfigurationsAreInvalid) {
  FeatureConfig foo_config =
      configuration_.GetFeatureConfig(kSingleTestFeatureFoo);
  EXPECT_FALSE(foo_config.valid);

  FeatureConfig bar_config =
      configuration_.GetFeatureConfig(kSingleTestFeatureBar);
  EXPECT_FALSE(bar_config.valid);
}

}  // namespace feature_engagement
