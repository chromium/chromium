// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/channel/channel_info.h"

#include <string>

#include "base/check_op.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ChannelInfoTest = testing::Test;

TEST_F(ChannelInfoTest, GetChannel) {
  constexpr char kLsbRelease[] = "CHROMEOS_RELEASE_TRACK=canary-channel";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(version_info::Channel::CANARY, GetChannel());
#else
  EXPECT_EQ(version_info::Channel::UNKNOWN, GetChannel());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

TEST_F(ChannelInfoTest, GetChannelName) {
  constexpr char kLsbRelease[] = "CHROMEOS_RELEASE_TRACK=dev-channel";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ("dev", GetChannelName());
#else
  EXPECT_EQ(std::string(), GetChannelName());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace ash
