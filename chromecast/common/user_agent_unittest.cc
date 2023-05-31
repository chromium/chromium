// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/user_agent.h"

#include "base/strings/stringprintf.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

namespace {
#if BUILDFLAG(IS_ANDROID)
const char kCastAndroid[] =
    "Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/%s.0.0.0 "
    "%sSafari/537.36 CrKey/1.56.500000 %s";
#else
const char kCastDesktop[] =
    "Mozilla/5.0 ("
#if BUILDFLAG(IS_CHROMEOS)
    "X11; CrOS x86_64 14541.0.0"
#elif BUILDFLAG(IS_FUCHSIA)
    "Fuchsia"
#elif BUILDFLAG(IS_LINUX)
    "X11; Linux x86_64"
#elif BUILDFLAG(IS_MAC)
    "Macintosh; Intel Mac OS X 10_15_7"
#elif BUILDFLAG(IS_WIN)
    "Windows NT 10.0; Win64; x64"
#else
#error Unsupported platform
#endif
    ") AppleWebKit/537.36 (KHTML, like Gecko) Chrome/%s.0.0.0 "
    "Safari/537.36 CrKey/1.56.500000 %s";
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace

TEST(UserAgentTest, GetUserAgent) {
  std::string device_suffix = GetDeviceUserAgentSuffix();
#if BUILDFLAG(IS_ANDROID)
  std::string device_compat = "";
  EXPECT_EQ(base::StringPrintf(kCastAndroid,
                               version_info::GetMajorVersionNumber().c_str(),
                               device_compat.c_str(), device_suffix.c_str()),
            GetUserAgent());
#else
  EXPECT_EQ(base::StringPrintf(kCastDesktop,
                               version_info::GetMajorVersionNumber().c_str(),
                               device_suffix.c_str()),
            GetUserAgent());
#endif
}

}  // namespace chromecast
