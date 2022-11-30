// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/http/user_agent.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

TEST(UserAgentTest, BasicTest) {
  UserAgent user_agent(L"product", L"1.0");

  user_agent.set_os_version(11, 13);
  user_agent.set_winhttp_version(L"super_duper");
  user_agent.set_architecture(UserAgent::WOW64);

  EXPECT_EQ(L"product/1.0 (Windows NT 11.13; WOW64) WinHTTP/super_duper",
            user_agent.AsString());
}

}  // namespace chrome_cleaner
