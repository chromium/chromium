// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_composebox_fieldtrial.h"

#include <string>

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/ntp_composebox_config.pb.h"

namespace ntp_composebox_fieldtrial {

namespace {

std::string SerializeAndBase64EncodeProto(
    const google::protobuf::MessageLite& proto) {
  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);
  return base::Base64Encode(serialized_proto);
}

}  // namespace

class NtpComposeboxFieldTrialTest : public testing::Test {
 public:
  ScopedFeatureConfigForTesting scoped_config_;
};

// Tests the NTP Composebox configuration when the feature is disabled.
TEST_F(NtpComposeboxFieldTrialTest, NTPComposeboxConfig_Disabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kNtpComposebox);

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  EXPECT_FALSE(scoped_config_.Get().enabled);
  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  EXPECT_EQ(config.entry_point().num_page_load_animations(), 3);

  histogram_tester.ExpectTotalCount(kConfigParamParseSuccessHistogram, 0);
}

// Tests the NTP Composebox configuration when the feature is enabled with the
// default parameter.
TEST_F(NtpComposeboxFieldTrialTest,
       NTPComposeboxConfig_Enabled_DefaultConfiguration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kNtpComposebox);

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  EXPECT_TRUE(scoped_config_.Get().enabled);
  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  EXPECT_EQ(config.entry_point().num_page_load_animations(), 3);

  histogram_tester.ExpectTotalCount(kConfigParamParseSuccessHistogram, 0);
}

// Tests the NTP Composebox configuration when the feature is enabled with an
// invalid Base64-encoded proto parameter.
TEST_F(NtpComposeboxFieldTrialTest,
       NTPComposeboxConfig_Enabled_Invalid_Configuration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kNtpComposebox, {{kConfigParam.name, "hello world"}});

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  EXPECT_EQ(config.entry_point().num_page_load_animations(), 3);

  histogram_tester.ExpectTotalCount(kConfigParamParseSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(kConfigParamParseSuccessHistogram, false,
                                     1);
}

// Tests the NTP Composebox configuration when the feature is enabled with a
// valid Base64-encoded proto parameter that does not set a value.
TEST_F(NtpComposeboxFieldTrialTest,
       NTPComposeboxConfig_Enabled_Valid_Unset_Configuration) {
  omnibox::NTPComposeboxConfig fieldtrial_config;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kNtpComposebox,
      {{kConfigParam.name, SerializeAndBase64EncodeProto(fieldtrial_config)}});

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  EXPECT_EQ(config.entry_point().num_page_load_animations(), 3);

  histogram_tester.ExpectTotalCount(kConfigParamParseSuccessHistogram, 0);
}

// Tests the NTP Composebox configuration when the feature is enabled with a
// valid Base64-encoded proto parameter that sets a value.
TEST_F(NtpComposeboxFieldTrialTest,
       NTPComposeboxConfig_Enabled_Valid_Set_Configuration) {
  omnibox::NTPComposeboxConfig fieldtrial_config;
  fieldtrial_config.mutable_entry_point()->set_num_page_load_animations(5);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kNtpComposebox,
      {{kConfigParam.name, SerializeAndBase64EncodeProto(fieldtrial_config)}});

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  EXPECT_TRUE(scoped_config_.Get().enabled);
  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  EXPECT_EQ(config.entry_point().num_page_load_animations(), 5);

  histogram_tester.ExpectTotalCount(kConfigParamParseSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(kConfigParamParseSuccessHistogram, true,
                                     1);
}

}  // namespace ntp_composebox_fieldtrial
