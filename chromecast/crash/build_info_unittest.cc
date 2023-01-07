// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/build_info.h"

#include <string>

#include "chromecast/base/version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

constexpr char kIncrementalUserBuild[] = "1.51.user.ldap.20200813.124713";
constexpr char kIncrementalEngBuild[] = "1.51.eng.ldap.20200813.124713";
constexpr char kReleaseBuild[] = "1.51.224083";

TEST(VersionStringTest, VersionStringIsNonEmpty) {
  ASSERT_FALSE(GetVersionString().empty());
}

TEST(VersionStringTest, VersionStringIsCorrect) {
  ASSERT_EQ(GetVersionString(kReleaseBuild, kIncrementalEngBuild),
            "1.51.224083.1.51.eng");
}

TEST(VersionStringTest, EmptyVersionStringIsCorrect) {
  ASSERT_FALSE(GetVersionString("", "").empty());
}

TEST(VersionStringTest, ReleaseVersionUnchanged) {
  ASSERT_EQ(VersionToCrashString(kReleaseBuild), kReleaseBuild);
}

TEST(VersionStringTest, IncrementalVersionTruncated) {
  ASSERT_EQ(VersionToCrashString(kIncrementalEngBuild), "1.51.eng");
  ASSERT_EQ(VersionToCrashString(kIncrementalUserBuild), "1.51.user");
}

TEST(GetBuildVariantTest, InfixPresent) {
  ASSERT_EQ(VersionToVariant(kIncrementalEngBuild), "eng");
  ASSERT_EQ(VersionToVariant(kIncrementalUserBuild), "user");
}

TEST(GetBuildVariantTest, InfixAbsent) {
  // If infix is absent, will use debug macro to determine variant.
  bool is_eng = CAST_IS_DEBUG_BUILD();
  ASSERT_EQ(VersionToVariant(kReleaseBuild) == "eng", is_eng);
}

}  // namespace chromecast
