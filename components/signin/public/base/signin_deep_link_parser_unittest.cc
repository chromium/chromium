// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_deep_link_parser.h"

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "components/signin/public/base/signin_deep_link_payload.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace signin {
namespace {
const char kDeepLinkBaseUrl[] = "https://www.google.com/chrome/go-mobile";

struct ParseTestParam {
  const std::string_view test_name;
  const std::string_view deep_link_url;
  const std::optional<SigninDeepLinkPayload> expected_payload;
};
}  // namespace

class SigninDeepLinkParserTest : public testing::TestWithParam<ParseTestParam> {
};

TEST_P(SigninDeepLinkParserTest, Parse) {
  const SigninDeepLinkParser parser{GURL(kDeepLinkBaseUrl)};
  const ParseTestParam& param = GetParam();
  const std::optional<SigninDeepLinkPayload> parsed_payload =
      parser.Parse(GURL(param.deep_link_url));

  EXPECT_THAT(parsed_payload, testing::Eq(param.expected_payload));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SigninDeepLinkParserTest,
    testing::Values(
        ParseTestParam{
            .test_name = "AllParametersPresent",
            .deep_link_url = "https://www.google.com/chrome/"
                             "go-mobile?entry_point_id=1&email=test@gmail.com",
            .expected_payload =
                SigninDeepLinkPayload{
                    .entry_point_id = ExternalEntryPoint::kDesktopDefault,
                    .entry_point_id_raw_value_for_metrics = 1,
                    .email = "test@gmail.com"},
        },
        ParseTestParam{
            .test_name = "InvalidDeepLinkWithTrailingSlashBeforeQueryParams",
            .deep_link_url = "https://www.google.com/chrome/"
                             "go-mobile/?entry_point_id=1&email=test@gmail.com",
            .expected_payload = std::nullopt,
        },
        ParseTestParam{
            .test_name = "EmailParameterPresentOnly",
            .deep_link_url =
                "https://www.google.com/chrome/go-mobile?email=test@gmail.com",
            .expected_payload =
                SigninDeepLinkPayload{
                    .entry_point_id = std::nullopt,
                    .entry_point_id_raw_value_for_metrics = std::nullopt,
                    .email = "test@gmail.com"},
        },
        ParseTestParam{
            .test_name = "EncodedEmail",
            .deep_link_url = "https://www.google.com/chrome/"
                             "go-mobile?email=test%40gmail.com",
            .expected_payload =
                SigninDeepLinkPayload{
                    .entry_point_id = std::nullopt,
                    .entry_point_id_raw_value_for_metrics = std::nullopt,
                    .email = "test@gmail.com"},
        },
        ParseTestParam{
            .test_name = "EntryPointIdParameterPresentOnly",
            .deep_link_url =
                "https://www.google.com/chrome/go-mobile?entry_point_id=1",
            .expected_payload =
                SigninDeepLinkPayload{
                    .entry_point_id = ExternalEntryPoint::kDesktopDefault,
                    .entry_point_id_raw_value_for_metrics = 1,
                    .email = std::nullopt},
        },
        ParseTestParam{
            .test_name = "NoParametersPresent",
            .deep_link_url = "https://www.google.com/chrome/go-mobile",
            .expected_payload =
                SigninDeepLinkPayload{
                    .entry_point_id = std::nullopt,
                    .entry_point_id_raw_value_for_metrics = std::nullopt,
                    .email = std::nullopt},
        },
        ParseTestParam{
            .test_name = "UnknownEntryPointId",
            .deep_link_url =
                "https://www.google.com/chrome/"
                "go-mobile?entry_point_id=1000&email=test@gmail.com",
            .expected_payload =
                SigninDeepLinkPayload{
                    .entry_point_id = ExternalEntryPoint::kUnknown,
                    .entry_point_id_raw_value_for_metrics = 1000,
                    .email = "test@gmail.com"},
        },
        ParseTestParam{
            .test_name = "EntryPointIdNotInteger",
            .deep_link_url =
                "https://www.google.com/chrome/"
                "go-mobile?entry_point_id=abc&email=test@gmail.com",
            .expected_payload =
                SigninDeepLinkPayload{
                    .entry_point_id = std::nullopt,
                    .entry_point_id_raw_value_for_metrics = std::nullopt,
                    .email = "test@gmail.com"},
        },
        ParseTestParam{
            .test_name = "EmptyEmailParameter",
            .deep_link_url = "https://www.google.com/chrome/"
                             "go-mobile?email=&entry_point_id=1",
            .expected_payload =
                SigninDeepLinkPayload{
                    .entry_point_id = ExternalEntryPoint::kDesktopDefault,
                    .entry_point_id_raw_value_for_metrics = 1,
                    .email = std::nullopt},
        },
        ParseTestParam{
            .test_name = "NotMatchingUrlScheme",
            .deep_link_url = "http://www.google.com/chrome/go-mobile",
            .expected_payload = std::nullopt,
        },
        ParseTestParam{
            .test_name = "NotMatchingUrlHost",
            .deep_link_url = "https://different.com/chrome/go-mobile",
            .expected_payload = std::nullopt,
        },
        ParseTestParam{
            .test_name = "NotMatchingUrlPort",
            .deep_link_url = "https://www.google.com:8080/chrome/go-mobile",
            .expected_payload = std::nullopt,
        },
        ParseTestParam{
            .test_name = "NotMatchingUrlPath",
            .deep_link_url = "https://www.google.com/different/path",
            .expected_payload = std::nullopt,
        },
        ParseTestParam{
            .test_name = "InvalidDeepLink",
            .deep_link_url = "invalid_deep_link",
            .expected_payload = std::nullopt,
        },
        ParseTestParam{
            .test_name = "EmptyDeepLink",
            .deep_link_url = "",
            .expected_payload = std::nullopt,
        }),
    [](const testing::TestParamInfo<ParseTestParam>& info) {
      return std::string(info.param.test_name);
    });

class CrossDeviceSigninDeepLinkParserFactoryTest : public testing::Test {
 protected:
  void EnableCrossDeviceSigninFeature(const std::string& deep_link_url) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        switches::kCrossDeviceSignin, {{"url", deep_link_url}});
  }

  void DisableCrossDeviceSigninFeature() {
    scoped_feature_list_.InitAndDisableFeature(switches::kCrossDeviceSignin);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CrossDeviceSigninDeepLinkParserFactoryTest,
       FeatureEnabledWithValidRemoteConfiguration) {
  EnableCrossDeviceSigninFeature(kDeepLinkBaseUrl);
  const auto parser =
      SigninDeepLinkParser::CreateForCrossDeviceSigninIfEnabled();
  ASSERT_TRUE(parser.has_value());
  const GURL deep_link = GURL(base::StrCat(
      {kDeepLinkBaseUrl, "?entry_point_id=1&email=test@gmail.com"}));
  const std::optional<SigninDeepLinkPayload> parsed_payload =
      parser->Parse(deep_link);
  const SigninDeepLinkPayload expected_payload = {
      .entry_point_id = ExternalEntryPoint::kDesktopDefault,
      .entry_point_id_raw_value_for_metrics = 1,
      .email = "test@gmail.com"};
  EXPECT_THAT(parsed_payload, testing::Eq(expected_payload));
}

TEST_F(CrossDeviceSigninDeepLinkParserFactoryTest,
       FeatureEnabledWithEmptyUrlInRemoteConfiguration) {
  EnableCrossDeviceSigninFeature("");
  const auto parser =
      SigninDeepLinkParser::CreateForCrossDeviceSigninIfEnabled();
  ASSERT_FALSE(parser.has_value());
}

TEST_F(CrossDeviceSigninDeepLinkParserFactoryTest,
       FeatureEnabledWithInvalidUrlInRemoteConfiguration) {
  EnableCrossDeviceSigninFeature("invalid.url/test");
  const auto parser =
      SigninDeepLinkParser::CreateForCrossDeviceSigninIfEnabled();
  ASSERT_FALSE(parser.has_value());
}

TEST_F(CrossDeviceSigninDeepLinkParserFactoryTest, FeatureDisabled) {
  DisableCrossDeviceSigninFeature();
  const auto parser =
      SigninDeepLinkParser::CreateForCrossDeviceSigninIfEnabled();
  ASSERT_FALSE(parser.has_value());
}

}  // namespace signin
