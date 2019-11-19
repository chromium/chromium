// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/mime_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace {
using MimeUtilsTest = testing::Test;

TEST_F(MimeUtilsTest, LegacyString) {
  std::string mime_type("UTF8_STRING");
  std::string expected("UTF-8");
  EXPECT_EQ(GetCharset(mime_type), expected);
}

TEST_F(MimeUtilsTest, CharsetNotPresent) {
  std::string mime_type("text/plain");
  std::string expected("US-ASCII");
  EXPECT_EQ(GetCharset(mime_type), expected);
}

TEST_F(MimeUtilsTest, CharsetPresent) {
  std::string mime_type("text/plain;charset=SomeCharacterSet");
  std::string expected("SomeCharacterSet");
  EXPECT_EQ(GetCharset(mime_type), expected);
}

TEST_F(MimeUtilsTest, CharsetHTML) {
  std::string mime_type("text/html;charset=SomeCharacterSet");
  std::string expected("SomeCharacterSet");
  EXPECT_EQ(GetCharset(mime_type), expected);
}

}  // namespace
}  // namespace exo
