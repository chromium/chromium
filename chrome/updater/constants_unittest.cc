// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/constants.h"

#include <string>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(ConstantsTest, PolicyManagerSourcesStable) {
  // These strings are persisted to event history logs and included in Mojo
  // responses. They should remain stable.
  EXPECT_EQ(kSourceDMPolicyManager, std::string("Device Management"));
  EXPECT_EQ(kSourceDefaultValuesPolicyManager, std::string("Default"));
  EXPECT_EQ(kSourceDictValuesPolicyManager, std::string("DictValuePolicy"));

#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(kSourcePlatformPolicyManager, std::string("Group Policy"));
#elif BUILDFLAG(IS_MAC)
  EXPECT_EQ(kSourcePlatformPolicyManager, std::string("Managed Preferences"));
#else
  EXPECT_EQ(kSourcePlatformPolicyManager, std::string("not-defined"));
#endif
}

}  // namespace updater
