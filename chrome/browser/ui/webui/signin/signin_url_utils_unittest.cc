// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_url_utils.h"

#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
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
  EXPECT_EQ(url.GetHost(), chrome::kChromeUIProfileCustomizationHost);
  EXPECT_EQ(ProfileCustomizationStyle::kDefault,
            GetProfileCustomizationStyle(url));

  // Profile Creation version.
  url = AppendProfileCustomizationQueryParams(
      GURL(chrome::kChromeUIProfileCustomizationURL),
      ProfileCustomizationStyle::kLocalProfileCreation);
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(url.GetHost(), chrome::kChromeUIProfileCustomizationHost);
  EXPECT_EQ(ProfileCustomizationStyle::kLocalProfileCreation,
            GetProfileCustomizationStyle(url));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
