// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdlib.h>
#include <windows.h>

#include <string>

#include "chrome/updater/win/installer/string.h"
#include "testing/gtest/include/gtest/gtest.h"

using updater::StackString;

namespace {
class InstallerStringTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};
}  // namespace

// Tests the strcat/strcpy/length support of the StackString class.
TEST_F(InstallerStringTest, StackStringOverflow) {
  static const wchar_t kTestString[] = L"1234567890";

  StackString<MAX_PATH> str;
  EXPECT_EQ(static_cast<size_t>(MAX_PATH), str.capacity());

  std::wstring compare_str;

  EXPECT_EQ(str.length(), compare_str.length());
  EXPECT_EQ(0, compare_str.compare(str.get()));

  size_t max_chars = str.capacity() - 1;

  while ((str.length() + (_countof(kTestString) - 1)) <= max_chars) {
    EXPECT_TRUE(str.append(kTestString));
    compare_str.append(kTestString);
    EXPECT_EQ(str.length(), compare_str.length());
    EXPECT_EQ(0, compare_str.compare(str.get()));
  }

  EXPECT_GT(static_cast<size_t>(MAX_PATH), str.length());

  // Now we've exhausted the space we allocated for the string,
  // so append should fail.
  EXPECT_FALSE(str.append(kTestString));

  // ...and remain unchanged.
  EXPECT_EQ(0, compare_str.compare(str.get()));
  EXPECT_EQ(str.length(), compare_str.length());

  // Last test for fun.
  str.clear();
  compare_str.clear();
  EXPECT_EQ(0, compare_str.compare(str.get()));
  EXPECT_EQ(str.length(), compare_str.length());
}
