// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_url_utils.h"

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

MATCHER_P(ParamsEq, expected_params, "") {
  return arg.is_modal == expected_params.is_modal &&
         arg.design == expected_params.design &&
         arg.profile_color == expected_params.profile_color;
}

// Tests that the default values correspond to the usage of the sync
// confirmation page in the modal flow.
TEST(SigninURLUtilsTest, SyncConfirmationURLParamsDefault) {
  SyncConfirmationURLParams default_params;
  SyncConfirmationURLParams expected_params = {
      /*is_modal=*/true, SyncConfirmationUI::DesignVersion::kMonotone,
      absl::nullopt};
  EXPECT_THAT(default_params, ParamsEq(expected_params));
}

TEST(SigninURLUtilsTest, ParseParameterlessSyncConfirmationURL) {
  GURL url = GURL(chrome::kChromeUISyncConfirmationURL);
  SyncConfirmationURLParams parsed_params =
      GetParamsFromSyncConfirmationURL(url);
  // Default params are expected.
  SyncConfirmationURLParams expected_params;
  EXPECT_THAT(parsed_params, ParamsEq(expected_params));
}

class SigninURLUtilsSyncConfirmationURLTest
    : public ::testing::TestWithParam<SyncConfirmationURLParams> {};

TEST_P(SigninURLUtilsSyncConfirmationURLTest, GetAndParseURL) {
  SyncConfirmationURLParams params = GetParam();
  GURL url = AppendSyncConfirmationQueryParams(
      GURL(chrome::kChromeUISyncConfirmationURL), params);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUISyncConfirmationHost);
  SyncConfirmationURLParams parsed_params =
      GetParamsFromSyncConfirmationURL(url);
  EXPECT_THAT(parsed_params, ParamsEq(params));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SigninURLUtilsSyncConfirmationURLTest,
    ::testing::Values(
        SyncConfirmationURLParams{},
        SyncConfirmationURLParams{
            true, SyncConfirmationUI::DesignVersion::kMonotone, absl::nullopt},
        SyncConfirmationURLParams{
            false, SyncConfirmationUI::DesignVersion::kMonotone, absl::nullopt},
        SyncConfirmationURLParams{
            false, SyncConfirmationUI::DesignVersion::kColored, absl::nullopt},
        SyncConfirmationURLParams{false,
                                  SyncConfirmationUI::DesignVersion::kColored,
                                  SK_ColorTRANSPARENT},
        SyncConfirmationURLParams{
            false, SyncConfirmationUI::DesignVersion::kColored, SK_ColorBLACK},
        SyncConfirmationURLParams{
            false, SyncConfirmationUI::DesignVersion::kColored, SK_ColorWHITE},
        SyncConfirmationURLParams{
            false, SyncConfirmationUI::DesignVersion::kColored, SK_ColorRED},
        SyncConfirmationURLParams{
            false, SyncConfirmationUI::DesignVersion::kColored, SK_ColorCYAN},
        SyncConfirmationURLParams{
            true, SyncConfirmationUI::DesignVersion::kColored, SK_ColorBLACK}));

class SigninURLUtilsReauthConfirmationURLTest
    : public ::testing::TestWithParam<int> {};

TEST_P(SigninURLUtilsReauthConfirmationURLTest,
       GetAndParseReauthConfirmationURL) {
  auto access_point =
      static_cast<signin_metrics::ReauthAccessPoint>(GetParam());
  GURL url = GetReauthConfirmationURL(access_point);
  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUISigninReauthHost);
  signin_metrics::ReauthAccessPoint get_access_point =
      GetReauthAccessPointForReauthConfirmationURL(url);
  EXPECT_EQ(get_access_point, access_point);
}

INSTANTIATE_TEST_SUITE_P(
    AllAccessPoints,
    SigninURLUtilsReauthConfirmationURLTest,
    ::testing::Range(
        static_cast<int>(signin_metrics::ReauthAccessPoint::kUnknown),
        static_cast<int>(signin_metrics::ReauthAccessPoint::kMaxValue) + 1));
