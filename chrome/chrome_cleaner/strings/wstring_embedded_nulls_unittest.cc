// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/strings/wstring_embedded_nulls.h"

#include <vector>

#include "base/strings/string_piece.h"
#include "chrome/chrome_cleaner/strings/string_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

constexpr wchar_t kStringWithNulls[] = L"string0with0nulls";

std::wstring FormatWStringPiece(base::WStringPiece sp) {
  return FormatVectorWithNulls(std::vector<wchar_t>(sp.begin(), sp.end()));
}

class WStringEmbeddedNullsTest : public ::testing::Test {
 protected:
  WStringEmbeddedNullsTest() : vec_(CreateVectorWithNulls(kStringWithNulls)) {}

  const std::vector<wchar_t> vec_;
};

void ExpectEmpty(const WStringEmbeddedNulls& str) {
  EXPECT_EQ(base::WStringPiece(), str.CastAsWStringPiece());
  EXPECT_EQ(0u, str.size());
  EXPECT_FALSE(str.CastAsUInt16Array());
}

TEST_F(WStringEmbeddedNullsTest, Empty) {
  ExpectEmpty(WStringEmbeddedNulls());
  ExpectEmpty(WStringEmbeddedNulls(nullptr));
  ExpectEmpty(WStringEmbeddedNulls(nullptr, 0));
  ExpectEmpty(WStringEmbeddedNulls(nullptr, 1));
  ExpectEmpty(WStringEmbeddedNulls(L"", 0));
  ExpectEmpty(WStringEmbeddedNulls(std::vector<wchar_t>{}));
  ExpectEmpty(WStringEmbeddedNulls(std::wstring()));
  ExpectEmpty(WStringEmbeddedNulls(std::wstring(L"")));
  ExpectEmpty(WStringEmbeddedNulls(base::WStringPiece()));
  ExpectEmpty(WStringEmbeddedNulls(base::WStringPiece(L"")));
}

TEST_F(WStringEmbeddedNullsTest, FromWCharArray) {
  WStringEmbeddedNulls str(vec_.data(), vec_.size());
  base::WStringPiece sp = str.CastAsWStringPiece();
  EXPECT_EQ(FormatVectorWithNulls(vec_), FormatWStringPiece(sp));
}

TEST_F(WStringEmbeddedNullsTest, FromVector) {
  WStringEmbeddedNulls str(vec_);
  base::WStringPiece sp = str.CastAsWStringPiece();
  EXPECT_EQ(FormatVectorWithNulls(vec_), FormatWStringPiece(sp));
}

TEST_F(WStringEmbeddedNullsTest, FromWString) {
  WStringEmbeddedNulls str(std::wstring(vec_.data(), vec_.size()));
  base::WStringPiece sp = str.CastAsWStringPiece();
  EXPECT_EQ(FormatVectorWithNulls(vec_), FormatWStringPiece(sp));
}

TEST_F(WStringEmbeddedNullsTest, FromWStringPiece) {
  WStringEmbeddedNulls str(base::WStringPiece(vec_.data(), vec_.size()));
  base::WStringPiece sp = str.CastAsWStringPiece();
  EXPECT_EQ(FormatVectorWithNulls(vec_), FormatWStringPiece(sp));
}

TEST_F(WStringEmbeddedNullsTest, CopyConstructor) {
  WStringEmbeddedNulls str(vec_.data(), vec_.size());
  WStringEmbeddedNulls copied_by_constructor(str);
  EXPECT_EQ(str.CastAsWStringPiece(),
            copied_by_constructor.CastAsWStringPiece());
}

TEST_F(WStringEmbeddedNullsTest, AssigmentOperator) {
  WStringEmbeddedNulls str(vec_.data(), vec_.size());

  WStringEmbeddedNulls other;
  EXPECT_EQ(base::WStringPiece(), other.CastAsWStringPiece());
  EXPECT_NE(str.CastAsWStringPiece(), other.CastAsWStringPiece());

  other = str;
  EXPECT_EQ(str.CastAsWStringPiece(), other.CastAsWStringPiece());
}

}  // namespace

}  // namespace chrome_cleaner
