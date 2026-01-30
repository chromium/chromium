// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/isolation_support.h"

#include <string>

#include "build/branding_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

TEST(IsolationSupportTest, ExpectedValue) {
  // Note: The isolation attribute name is not channel specific.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr const wchar_t* expected = L"GOOGLECHROME://ISOLATION";
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
  static constexpr const wchar_t* expected =
      L"GOOGLECHROMEFORTESTING://ISOLATION";
#else
  static constexpr const wchar_t* expected = L"CHROMIUM://ISOLATION";
#endif

  EXPECT_THAT(GetIsolationAttributeName(), ::testing::StrCaseEq(expected));
}

}  // namespace installer
