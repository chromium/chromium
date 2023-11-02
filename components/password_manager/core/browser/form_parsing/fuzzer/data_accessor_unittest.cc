// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/fuzzer/data_accessor.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UTF8ToUTF16;

namespace password_manager {

namespace {

TEST(DataAccessorTest, NullInput) {
  DataAccessor accessor(nullptr, 0);
  EXPECT_EQ(0u, accessor.ConsumeNumber(13));
  EXPECT_EQ(false, accessor.ConsumeBit());
  EXPECT_EQ(std::string("\0\0\0", 3), accessor.ConsumeString(3));
  EXPECT_EQ(std::u16string(), accessor.ConsumeString16(0));
}

TEST(DataAccessorTest, Bit) {
  const uint8_t x = 0b10110001;
  DataAccessor accessor(&x, 1);
  EXPECT_EQ(true, accessor.ConsumeBit());
  EXPECT_EQ(false, accessor.ConsumeBit());
  EXPECT_EQ(false, accessor.ConsumeBit());
  EXPECT_EQ(false, accessor.ConsumeBit());
  EXPECT_EQ(true, accessor.ConsumeBit());
  EXPECT_EQ(true, accessor.ConsumeBit());
  EXPECT_EQ(false, accessor.ConsumeBit());
  EXPECT_EQ(true, accessor.ConsumeBit());
}

TEST(DataAccessorTest, Number) {
  const uint8_t xs[] = {0b01100110, 0b11100110};
  DataAccessor accessor(xs, sizeof(xs));
  accessor.ConsumeBit();  // Just skip the first bit for fun.
  EXPECT_EQ(0b011u, accessor.ConsumeNumber(3));
  EXPECT_EQ(0b0u, accessor.ConsumeNumber(1));
  EXPECT_EQ(0b11u, accessor.ConsumeNumber(2));
  // 10 (2nd byte) ++ 0 (1st byte):
  EXPECT_EQ(0b100u, accessor.ConsumeNumber(3));
  EXPECT_EQ(0u, accessor.ConsumeNumber(0));  // An empty string represents 0.
  EXPECT_EQ(0b11001u, accessor.ConsumeNumber(5));
  EXPECT_EQ(0b01u, accessor.ConsumeNumber(2));       // 1, also reaching padding
  EXPECT_EQ(0b0000000u, accessor.ConsumeNumber(7));  // padding
}

TEST(DataAccessorTest, String) {
  const std::string str = "Test string 123.";
  DataAccessor accessor(reinterpret_cast<const uint8_t*>(str.c_str()),
                        str.size());
  EXPECT_EQ("Test", accessor.ConsumeString(4));
  accessor.ConsumeNumber(3);  // Skip 3 bits to test re-alignment.
  EXPECT_EQ("string 123", accessor.ConsumeString(10));
  EXPECT_EQ(std::string(), accessor.ConsumeString(0));
  // Test also that padding is included.
  EXPECT_EQ(std::string(".\0\0", 3), accessor.ConsumeString(3));
}

TEST(DataAccessorTest, String16) {
  const std::u16string str = u"Test string 123.";
  DataAccessor accessor(reinterpret_cast<const uint8_t*>(str.c_str()),
                        str.size() * 2);
  EXPECT_EQ(u"Test", accessor.ConsumeString16(4));
  accessor.ConsumeNumber(13);  // Skip 13 bits to test re-alignment.
  EXPECT_EQ(u"string 123", accessor.ConsumeString16(10));
  EXPECT_EQ(std::u16string(), accessor.ConsumeString16(0));
  // Test also that padding is included.
  EXPECT_EQ(UTF8ToUTF16(std::string(".\0\0", 3)), accessor.ConsumeString16(3));
}

TEST(DataAccessorTest, Mix) {
  const uint8_t xs[] = {'a',        'b', 0b11100101, 5,   9,
                        0b10000001, 'c', 'd',        'e', 0};
  DataAccessor accessor(xs, sizeof(xs));
  EXPECT_EQ("ab", accessor.ConsumeString(2));
  EXPECT_EQ(true, accessor.ConsumeBit());
  EXPECT_EQ(0b1110010u, accessor.ConsumeNumber(7));
  EXPECT_EQ(5u, accessor.ConsumeNumber(8));
  EXPECT_EQ(9u + (1u << 8), accessor.ConsumeNumber(9));
  EXPECT_EQ(false, accessor.ConsumeBit());
  EXPECT_EQ("cd", accessor.ConsumeString(2));
  EXPECT_EQ(u"e", accessor.ConsumeString16(1));
}
}  // namespace

}  // namespace password_manager
