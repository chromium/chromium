// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/single_invalid_configuration.h"

#include "base/feature_list.h"
#include "components/feature_engagement/public/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

BASE_FEATURE(kSingleTestFeatureFoo,
             "test_foo",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSingleTestFeatureBar,
             "test_bar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSingleTestGroupBaz,
             "test_group_baz",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSingleTestGroupQux,
             "test_group_qux",
             base::FEATURE_DISABLED_BY_DEFAULT);

class SingleInvalidConfigurationTest : public ::testing::Test {
 public:
  SingleInvalidConfigurationTest() = default;

  SingleInvalidConfigurationTest(const SingleInvalidConfigurationTest&) =
      delete;
  SingleInvalidConfigurationTest& operator=(
      const SingleInvalidConfigurationTest&) = delete;

 protected:
  SingleInvalidConfiguration configuration_;
};

}  // namespace

TEST_F(SingleInvalidConfigurationTest, AllConfigurationsAreInvalid) {
  FeatureConfig foo_config =
      configuration_.GetFeatureConfig(kSingleTestFeatureFoo);
  EXPECT_FALSE(foo_config.valid);

  FeatureConfig bar_config =
      configuration_.GetFeatureConfig(kSingleTestFeatureBar);
  EXPECT_FALSE(bar_config.valid);

  GroupConfig baz_group_config =
      configuration_.GetGroupConfig(kSingleTestGroupBaz);
  EXPECT_FALSE(baz_group_config.valid);

  GroupConfig qux_group_config =
      configuration_.GetGroupConfig(kSingleTestGroupQux);
  EXPECT_FALSE(qux_group_config.valid);
}

}  // namespace feature_engagement
