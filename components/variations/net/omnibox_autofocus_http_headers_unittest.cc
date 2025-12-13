// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/omnibox_autofocus_http_headers.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace variations {

namespace {

struct GetHeaderValueTestParam {
  // Whether the main feature is enabled.
  bool feature_enabled;
  // A map of feature parameters.
  base::FieldTrialParams feature_params;
  // The expected header value.
  std::string expected_value;
};

class OmniboxAutofocusGetHeaderValueTest
    : public testing::Test,
      public testing::WithParamInterface<GetHeaderValueTestParam> {};

TEST_P(OmniboxAutofocusGetHeaderValueTest, GetHeaderValue) {
  const auto& param = GetParam();
  base::test::ScopedFeatureList feature_list;
  if (param.feature_enabled) {
    feature_list.InitAndEnableFeatureWithParameters(
        kOmniboxAutofocusOnIncognitoNtp, param.feature_params);
  } else {
    feature_list.InitAndDisableFeature(kOmniboxAutofocusOnIncognitoNtp);
  }
  EXPECT_EQ(GetHeaderValue(), param.expected_value);
}

INSTANTIATE_TEST_SUITE_P(
    OmniboxAutofocusHttpHeadersTest,
    OmniboxAutofocusGetHeaderValueTest,
    testing::ValuesIn(std::vector<GetHeaderValueTestParam>{
        // Test with kOmniboxAutofocusOnIncognitoNtp disabled.
        {false, {}, "-1"},
        // Test with kOmniboxAutofocusOnIncognitoNtp enabled and no params set.
        {true, {}, "0"},
        // Test with kOmniboxAutofocusOnIncognitoNtp enabled and all params
        // explicitly set to false.
        {true,
         {{"not_first_tab", "false"},
          {"with_prediction", "false"},
          {"with_hardware_keyboard", "false"}},
         "0"},
        // bit 0: with_hardware_keyboard
        {true, {{"with_hardware_keyboard", "true"}}, "1"},
        // bit 1: with_prediction
        {true, {{"with_prediction", "true"}}, "2"},
        // bit 0 and 1
        {true,
         {{"with_prediction", "true"}, {"with_hardware_keyboard", "true"}},
         "3"},
        // bit 2: not_first_tab
        {true, {{"not_first_tab", "true"}}, "4"},
        // bit 2 and 0
        {true,
         {{"not_first_tab", "true"}, {"with_hardware_keyboard", "true"}},
         "5"},
        // bit 2 and 1
        {true, {{"not_first_tab", "true"}, {"with_prediction", "true"}}, "6"},
        // bit 2, 1, and 0
        {true,
         {{"not_first_tab", "true"},
          {"with_prediction", "true"},
          {"with_hardware_keyboard", "true"}},
         "7"},
    }));

}  // namespace

TEST(OmniboxAutofocusHttpHeadersTest,
     UpdateCorsExemptHeaderForOmniboxAutofocus) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kReportOmniboxAutofocusHeader);

  network::mojom::NetworkContextParams params;
  UpdateCorsExemptHeaderForOmniboxAutofocus(&params);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(params.cors_exempt_header_list.size(), 1u);
  EXPECT_EQ(params.cors_exempt_header_list[0], kOmniboxAutofocusHeaderName);
#else
  EXPECT_TRUE(params.cors_exempt_header_list.empty());
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST(OmniboxAutofocusHttpHeadersTest, ShouldAppendHeader) {
  EXPECT_TRUE(ShouldAppendHeader(GURL("https://www.google.com")));
  EXPECT_TRUE(ShouldAppendHeader(GURL("https://www.search.google.com")));
  EXPECT_FALSE(ShouldAppendHeader(GURL("https://www.youtube.com")));
  EXPECT_FALSE(ShouldAppendHeader(GURL("http://www.google.com")));
  EXPECT_FALSE(ShouldAppendHeader(GURL("https://www.not-google.com")));
}

#if BUILDFLAG(IS_ANDROID)
TEST(OmniboxAutofocusHttpHeadersTest,
     ShouldNotAppendHeader_WhenReportingFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kReportOmniboxAutofocusHeader);

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com");
  AppendOmniboxAutofocusHeaderIfNeeded(request.url, &request);
  EXPECT_FALSE(
      request.cors_exempt_headers.HasHeader(kOmniboxAutofocusHeaderName));
}

TEST(OmniboxAutofocusHttpHeadersTest, ShouldAppendHeader_ForControlGroup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kReportOmniboxAutofocusHeader);

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com");
  AppendOmniboxAutofocusHeaderIfNeeded(request.url, &request);
  std::optional<std::string> header_value =
      request.cors_exempt_headers.GetHeader(kOmniboxAutofocusHeaderName);
  ASSERT_TRUE(header_value.has_value());
  EXPECT_EQ(*header_value, "-1");
}

TEST(OmniboxAutofocusHttpHeadersTest, ShouldAppendHeader_ForTreatmentGroup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kReportOmniboxAutofocusHeader, {}},
       {kOmniboxAutofocusOnIncognitoNtp, {{"not_first_tab", "true"}}}},
      {});

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com");
  AppendOmniboxAutofocusHeaderIfNeeded(request.url, &request);
  std::optional<std::string> header_value =
      request.cors_exempt_headers.GetHeader(kOmniboxAutofocusHeaderName);
  ASSERT_TRUE(header_value.has_value());
  EXPECT_EQ(*header_value, "4");
}

TEST(OmniboxAutofocusHttpHeadersTest, ShouldNotAppendHeader_ForNonGoogleUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kReportOmniboxAutofocusHeader);

  network::ResourceRequest request;
  request.url = GURL("https://www.not-google.com");
  AppendOmniboxAutofocusHeaderIfNeeded(request.url, &request);
  EXPECT_FALSE(
      request.cors_exempt_headers.HasHeader(kOmniboxAutofocusHeaderName));
}

#else

TEST(OmniboxAutofocusHttpHeadersTest, ShoulNotdAppendHeader_IfNotAndroid) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kReportOmniboxAutofocusHeader, {}},
       {kOmniboxAutofocusOnIncognitoNtp, {{"not_first_tab", "true"}}}},
      {});

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com");
  AppendOmniboxAutofocusHeaderIfNeeded(request.url, &request);
  std::optional<std::string> header_value =
      request.cors_exempt_headers.GetHeader(kOmniboxAutofocusHeaderName);
  EXPECT_FALSE(header_value.has_value());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace variations
