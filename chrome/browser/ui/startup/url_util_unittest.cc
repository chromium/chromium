// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/url_util.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace startup {

TEST(UrlUtilTest, ValidateLaunchUrl) {
  EXPECT_TRUE(ValidateLaunchUrl(GURL("http://google.com")));
  EXPECT_TRUE(ValidateLaunchUrl(GURL("https://google.com")));
  EXPECT_TRUE(ValidateLaunchUrl(GURL("file:///tmp/test")));
  EXPECT_TRUE(ValidateLaunchUrl(GURL("about:blank")));

  // chrome:// settings
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(ValidateLaunchUrl(GURL("chrome://settings")));
#else
  EXPECT_FALSE(ValidateLaunchUrl(GURL("chrome://settings")));
#endif

  // javascript
  EXPECT_FALSE(ValidateLaunchUrl(GURL("javascript:alert(1)")));

  // Invalid GURL
  EXPECT_FALSE(ValidateLaunchUrl(GURL("")));

#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(
      ValidateLaunchUrl(GURL("content://packagename.providername/path")));
#else
  EXPECT_FALSE(
      ValidateLaunchUrl(GURL("content://packagename.providername/path")));
#endif
}

}  // namespace startup
