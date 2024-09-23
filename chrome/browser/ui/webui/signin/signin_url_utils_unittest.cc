// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_url_utils.h"

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(SigninURLUtilsTest, ParseParameterlessSyncConfirmationURL) {
  GURL url = GURL(chrome::kChromeUISyncConfirmationURL);
  EXPECT_EQ(SyncConfirmationStyle::kDefaultModal,
            GetSyncConfirmationStyle(url));
}

TEST(SigninURLUtilsTest, AddAndGetFromProfilePickerURLParam) {
  // Create a basic url, e.g. chrome://signin-error/.
  GURL url(chrome::kChromeUISigninErrorURL);
  ASSERT_FALSE(HasFromProfilePickerURLParameter(url));

  // Append the profile picker tag
  url = AddFromProfilePickerURLParameter(url);

  // Checks that the raw url contains the tag.
  EXPECT_TRUE(url.spec().find("from_profile_picker=true"));
  EXPECT_TRUE(url.is_valid());
  // Test the getter function that checks the tag.
  EXPECT_TRUE(HasFromProfilePickerURLParameter(url));
}

TEST(SigninURLUtilsSyncConfirmationURLTest, GetAndParseURL) {
  // Modal version.
  GURL url = AppendSyncConfirmationQueryParams(
      GURL(chrome::kChromeUISyncConfirmationURL),
      SyncConfirmationStyle::kDefaultModal, /*is_sync_promo=*/false);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUISyncConfirmationHost);
  EXPECT_EQ(SyncConfirmationStyle::kDefaultModal,
            GetSyncConfirmationStyle(url));
  EXPECT_FALSE(IsSyncConfirmationPromo(url));

  // Signin Intercept version.
  url = AppendSyncConfirmationQueryParams(
      GURL(chrome::kChromeUISyncConfirmationURL),
      SyncConfirmationStyle::kSigninInterceptModal,
      /*is_sync_promo=*/false);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUISyncConfirmationHost);
  EXPECT_EQ(SyncConfirmationStyle::kSigninInterceptModal,
            GetSyncConfirmationStyle(url));
  EXPECT_FALSE(IsSyncConfirmationPromo(url));

  // Window version.
  url = AppendSyncConfirmationQueryParams(
      GURL(chrome::kChromeUISyncConfirmationURL),
      SyncConfirmationStyle::kWindow, /*is_sync_promo=*/false);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUISyncConfirmationHost);
  EXPECT_EQ(SyncConfirmationStyle::kWindow, GetSyncConfirmationStyle(url));
  EXPECT_FALSE(IsSyncConfirmationPromo(url));

  // Promo version
  url = AppendSyncConfirmationQueryParams(
      GURL(chrome::kChromeUISyncConfirmationURL),
      SyncConfirmationStyle::kWindow, /*is_sync_promo=*/true);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUISyncConfirmationHost);
  EXPECT_EQ(SyncConfirmationStyle::kWindow, GetSyncConfirmationStyle(url));
  EXPECT_TRUE(IsSyncConfirmationPromo(url));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
TEST(SigninURLUtilsTest, ParseParameterlessProfileCustomizationURL) {
  GURL url = GURL(chrome::kChromeUIProfileCustomizationURL);
  EXPECT_EQ(ProfileCustomizationStyle::kDefault,
            GetProfileCustomizationStyle(url));
}

TEST(SigninURLUtilsProfileCustomizationURLTest, GetAndParseURL) {
  // Default version.
  GURL url = AppendProfileCustomizationQueryParams(
      GURL(chrome::kChromeUIProfileCustomizationURL),
      ProfileCustomizationStyle::kDefault);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUIProfileCustomizationHost);
  EXPECT_EQ(ProfileCustomizationStyle::kDefault,
            GetProfileCustomizationStyle(url));

  // Profile Creation version.
  url = AppendProfileCustomizationQueryParams(
      GURL(chrome::kChromeUIProfileCustomizationURL),
      ProfileCustomizationStyle::kLocalProfileCreation);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url.host(), chrome::kChromeUIProfileCustomizationHost);
  EXPECT_EQ(ProfileCustomizationStyle::kLocalProfileCreation,
            GetProfileCustomizationStyle(url));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

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
