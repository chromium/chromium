// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/continuous_search/common/title_validator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace continuous_search {

TEST(TitleValidator, Trim) {
  EXPECT_EQ("NULL", ValidateTitleAscii("NULL"));
  EXPECT_EQ("a", ValidateTitleAscii(" a "));
  EXPECT_EQ("b", ValidateTitleAscii(" b"));
  EXPECT_EQ("c", ValidateTitleAscii("c "));
  EXPECT_EQ("", ValidateTitleAscii("     "));
  EXPECT_EQ("", ValidateTitleAscii(""));
  EXPECT_EQ("", ValidateTitleAscii("\0"));
  EXPECT_EQ("\v", ValidateTitleAscii(" \v "));
  // Tests all control characters.
  EXPECT_EQ(
      "FOO",
      ValidateTitleAscii(
          "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x7F\x0C\x0D\x0E\x0F"
          "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
          " FOO "
          "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
          "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x7F\x0C\x0D\x0E\x0F"));
  // Tests all whitespace characters.
  EXPECT_EQ(u"Bar", ValidateTitle(u"\xA0\x3000\t\r\f\nBar"));
}

TEST(TitleValidator, Collapse) {
  // Collapse control.
  EXPECT_EQ(
      "a b",
      ValidateTitleAscii(
          "a\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x7F\x0C\x0D\x0E\x0F"
          "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
          " "
          "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
          "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x7F\x0C\x0D\x0E\x0F b"));
  // Collapse whitespace.
  EXPECT_EQ(u"Foo Bar", ValidateTitle(u"Foo\xA0\x3000\t\r\f\nBar"));
}

TEST(TitleValidator, CollapseAndTrim) {
  EXPECT_EQ(
      "a b",
      ValidateTitleAscii(
          "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x7F\x0C\x0D\x0E\x0F"
          "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
          " a "
          "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
          "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x7F\x0C\x0D\x0E\x0F b"));
  EXPECT_EQ(u"Foo Bar", ValidateTitle(u"\nFoo\xA0\x3000\t\r\f\nBar\n"));
}

TEST(TitleValidator, MaxLengthExceeded) {
  EXPECT_EQ(std::string(4096, 'a'), ValidateTitleAscii(std::string(4098, 'a')));
  EXPECT_EQ(std::u16string(4096, 'b'),
            ValidateTitle(std::u16string(5012, 'b')));

  // With collapse.
  EXPECT_EQ(std::string(1000, 'a') + " " + std::string(3095, 'b'),
            ValidateTitleAscii(std::string(1000, 'a') + std::string(500, ' ') +
                               std::string(4000, 'b')));

  // With trim.
  EXPECT_EQ(std::string(2049, 'a') + std::string(2047, 'b'),
            ValidateTitleAscii(std::string(10, ' ') + std::string(2049, 'a') +
                               std::string(3000, 'b') + std::string(30, ' ')));

  // With collapse and trim.
  EXPECT_EQ(std::string(2049, 'a') + " " + std::string(2046, 'b'),
            ValidateTitleAscii(std::string(10, ' ') + std::string(2049, 'a') +
                               std::string(1000, ' ') + std::string(3000, 'b') +
                               std::string(30, ' ')));
}

TEST(TitleValidator, MaxLengthAvoided) {
  // With collapse.
  EXPECT_EQ(std::string(1000, 'a') + " " + std::string(1000, 'b'),
            ValidateTitleAscii(std::string(1000, 'a') + std::string(5000, ' ') +
                               std::string(1000, 'b')));

  // With trim.
  EXPECT_EQ(
      std::string(1024, 'a') + std::string(1024, 'b'),
      ValidateTitleAscii(std::string(5000, ' ') + std::string(1024, 'a') +
                         std::string(1024, 'b') + std::string(5000, ' ')));

  // With collapse and trim.
  EXPECT_EQ(
      std::u16string(2000, 'a') + u" " + std::u16string(2000, 'b'),
      ValidateTitle(std::u16string(5000, ' ') + std::u16string(2000, 'a') +
                    std::u16string(1000, ' ') + std::u16string(2000, 'b') +
                    std::u16string(5000, ' ')));
}

}  // namespace continuous_search
