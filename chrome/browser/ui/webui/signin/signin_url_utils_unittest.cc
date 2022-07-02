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
#include "url/gurl.h"

TEST(SigninURLUtilsTest, ParseParameterlessSyncConfirmationURL) {
  GURL url = GURL(chrome::kChromeUISyncConfirmationURL);
  EXPECT_TRUE(IsSyncConfirmationModal(url));
}

TEST(SigninURLUtilsSyncConfirmationURLTest, GetAndParseURL) {
  // Modal version.
  GURL url = AppendSyncConfirmationQueryParams(
      GURL(chrome::kChromeUISyncConfirmationURL), /*is_modal=*/true);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUISyncConfirmationHost);
  EXPECT_TRUE(IsSyncConfirmationModal(url));

  // Non-modal version.
  url = AppendSyncConfirmationQueryParams(
      GURL(chrome::kChromeUISyncConfirmationURL), /*is_modal=*/false);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUISyncConfirmationHost);
  EXPECT_FALSE(IsSyncConfirmationModal(url));
}

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
