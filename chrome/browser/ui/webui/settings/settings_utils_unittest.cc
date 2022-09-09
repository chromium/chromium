// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace settings_utils {

TEST(SettingsUtilsTest, FixupAndValidateStartupPage) {
  EXPECT_FALSE(FixupAndValidateStartupPage(std::string(), nullptr));
  EXPECT_FALSE(FixupAndValidateStartupPage("   ", nullptr));
  EXPECT_FALSE(FixupAndValidateStartupPage("^&*@)^)", nullptr));
  EXPECT_FALSE(FixupAndValidateStartupPage("chrome://quit", nullptr));

  EXPECT_TRUE(FixupAndValidateStartupPage("facebook.com", nullptr));
  EXPECT_TRUE(FixupAndValidateStartupPage("http://reddit.com", nullptr));
  EXPECT_TRUE(FixupAndValidateStartupPage("https://google.com", nullptr));
  EXPECT_TRUE(FixupAndValidateStartupPage("chrome://apps", nullptr));

  GURL fixed_url;
  EXPECT_TRUE(FixupAndValidateStartupPage("about:settings", &fixed_url));
  EXPECT_EQ("chrome://settings/", fixed_url.spec());
}

}  // namespace settings_utils
