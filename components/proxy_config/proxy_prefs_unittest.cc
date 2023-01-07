// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/values.h"
#include "components/proxy_config/proxy_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ProxyPrefsTest, StringToProxyMode) {
  ProxyPrefs::ProxyMode mode;
  EXPECT_TRUE(ProxyPrefs::StringToProxyMode("direct", &mode));
  EXPECT_EQ(ProxyPrefs::MODE_DIRECT, mode);
  EXPECT_TRUE(ProxyPrefs::StringToProxyMode("auto_detect", &mode));
  EXPECT_EQ(ProxyPrefs::MODE_AUTO_DETECT, mode);
  EXPECT_TRUE(ProxyPrefs::StringToProxyMode("pac_script", &mode));
  EXPECT_EQ(ProxyPrefs::MODE_PAC_SCRIPT, mode);
  EXPECT_TRUE(ProxyPrefs::StringToProxyMode("system", &mode));
  EXPECT_EQ(ProxyPrefs::MODE_SYSTEM, mode);
  EXPECT_TRUE(ProxyPrefs::StringToProxyMode("fixed_servers", &mode));
  EXPECT_EQ(ProxyPrefs::MODE_FIXED_SERVERS, mode);

  EXPECT_FALSE(ProxyPrefs::StringToProxyMode("monkey", &mode));
}

TEST(ProxyPrefsTest, IntToProxyMode) {
  ASSERT_EQ(ProxyPrefs::MODE_DIRECT, 0);
  ASSERT_EQ(ProxyPrefs::MODE_AUTO_DETECT, 1);
  ASSERT_EQ(ProxyPrefs::MODE_PAC_SCRIPT, 2);
  ASSERT_EQ(ProxyPrefs::MODE_FIXED_SERVERS, 3);
  ASSERT_EQ(ProxyPrefs::MODE_SYSTEM, 4);
  // Update the following as necessary, don't change the previous ones.
  ASSERT_EQ(ProxyPrefs::kModeCount, 5);

  ProxyPrefs::ProxyMode mode;
  EXPECT_TRUE(ProxyPrefs::IntToProxyMode(0, &mode));
  EXPECT_EQ(ProxyPrefs::MODE_DIRECT, mode);
  EXPECT_TRUE(ProxyPrefs::IntToProxyMode(1, &mode));
  EXPECT_EQ(ProxyPrefs::MODE_AUTO_DETECT, mode);
  EXPECT_TRUE(ProxyPrefs::IntToProxyMode(2, &mode));
  EXPECT_EQ(ProxyPrefs::MODE_PAC_SCRIPT, mode);
  EXPECT_TRUE(ProxyPrefs::IntToProxyMode(3, &mode));
  EXPECT_EQ(ProxyPrefs::MODE_FIXED_SERVERS, mode);
  EXPECT_TRUE(ProxyPrefs::IntToProxyMode(4, &mode));
  EXPECT_EQ(ProxyPrefs::MODE_SYSTEM, mode);

  EXPECT_FALSE(ProxyPrefs::IntToProxyMode(-1, &mode));
  EXPECT_FALSE(ProxyPrefs::IntToProxyMode(ProxyPrefs::kModeCount, &mode));
}
