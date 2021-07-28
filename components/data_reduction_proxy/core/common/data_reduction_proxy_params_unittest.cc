// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <stddef.h>

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/proxy_server.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace data_reduction_proxy {

class DataReductionProxyParamsTest : public testing::Test {};


TEST_F(DataReductionProxyParamsTest, PromoFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
  } tests[] = {
      {"Enabled", true},
      {"Enabled_Control", true},
      {"Disabled", false},
      {"enabled", false},
  };

  for (const auto& test : tests) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.Init();

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "DataCompressionProxyPromoVisibility", test.trial_group_name));
    EXPECT_EQ(test.expected_enabled, params::IsIncludedInPromoFieldTrial())
        << test.trial_group_name;
  }
}

TEST_F(DataReductionProxyParamsTest, FREPromoFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
  } tests[] = {
      {"Enabled", true},
      {"Enabled_Control", true},
      {"Disabled", false},
      {"enabled", false},
  };

  for (const auto& test : tests) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.Init();

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "DataReductionProxyFREPromo", test.trial_group_name));
    EXPECT_EQ(test.expected_enabled, params::IsIncludedInFREPromoFieldTrial())
        << test.trial_group_name;
  }
}

TEST_F(DataReductionProxyParamsTest, LowMemoryPromoFeature) {
  const struct {
    bool expected_in_field_trial;
  } tests[] = {
      {false}, {true},
  };

  for (const auto& test : tests) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (test.expected_in_field_trial) {
      scoped_feature_list.InitAndDisableFeature(
          features::kDataReductionProxyLowMemoryDevicePromo);
    } else {
      scoped_feature_list.InitAndEnableFeature(
          features::kDataReductionProxyLowMemoryDevicePromo);
    }

#if defined(OS_ANDROID)
    EXPECT_EQ(test.expected_in_field_trial && base::SysInfo::IsLowEndDevice(),
              params::IsIncludedInPromoFieldTrial());
    EXPECT_EQ(test.expected_in_field_trial && base::SysInfo::IsLowEndDevice(),
              params::IsIncludedInFREPromoFieldTrial());
#else
    EXPECT_FALSE(params::IsIncludedInPromoFieldTrial());
    EXPECT_FALSE(params::IsIncludedInFREPromoFieldTrial());
#endif
  }
}

}  // namespace data_reduction_proxy
