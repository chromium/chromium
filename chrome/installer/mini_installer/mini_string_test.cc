// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/mini_string.h"

#include <windows.h>

#include <stddef.h>
#include <stdlib.h>

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

using mini_installer::StackString;

namespace {
class MiniInstallerStringTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};
}  // namespace

// Tests the strcat/strcpy/length support of the StackString class.
TEST_F(MiniInstallerStringTest, StackStringOverflow) {
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

// Tests the case insensitive find support of the StackString class.
TEST_F(MiniInstallerStringTest, StackStringFind) {
  static const wchar_t kTestStringSource[] = L"1234ABcD567890";
  static const wchar_t kTestStringFind[] = L"abcd";
  static const wchar_t kTestStringNotFound[] = L"80";

  StackString<MAX_PATH> str;
  EXPECT_TRUE(str.assign(kTestStringSource));
  EXPECT_EQ(str.get(), str.findi(kTestStringSource));
  EXPECT_EQ(nullptr, str.findi(kTestStringNotFound));
  const wchar_t* found = str.findi(kTestStringFind);
  EXPECT_NE(nullptr, found);
  std::wstring check(found, _countof(kTestStringFind) - 1);
  EXPECT_EQ(0, lstrcmpi(check.c_str(), kTestStringFind));
}
