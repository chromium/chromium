// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password_iterator.h"

#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using ::autofill::PasswordForm;

TEST(CSVPasswordIteratorTest, Operations) {
  // Default.
  CSVPasswordIterator def;
  EXPECT_EQ(def, def);

  // From CSV.
  const CSVPassword::ColumnMap kColMap = {
      {0, CSVPassword::Label::kOrigin},
      {1, CSVPassword::Label::kUsername},
      {2, CSVPassword::Label::kPassword},
  };
  constexpr base::StringPiece kCSV = "http://example.com,user,password";
  CSVPasswordIterator iter(kColMap, kCSV);
  // Because kCSV is just one row, it can be used to create a CSVPassword
  // directly.
  EXPECT_EQ(iter->ParseValid(), CSVPassword(kColMap, kCSV).ParseValid());

  // Copy.
  CSVPasswordIterator copy = iter;
  EXPECT_EQ(copy, iter);

  // Assignment.
  CSVPasswordIterator target;
  target = iter;
  EXPECT_EQ(target, iter);
  copy = def;
  EXPECT_EQ(copy, def);

  // More of equality and increment.
  CSVPasswordIterator dummy;
  EXPECT_NE(dummy, iter);
  CSVPasswordIterator same_as_iter(kColMap, kCSV);
  EXPECT_EQ(same_as_iter, iter);
  const std::string kCSVCopy(kCSV);
  CSVPasswordIterator same_looking(kColMap, kCSVCopy);
  EXPECT_NE(same_looking, iter);
  CSVPasswordIterator old = iter++;
  EXPECT_NE(old, iter);
  EXPECT_EQ(++old, iter);
}

TEST(CSVPasswordIteratorTest, Success) {
  const CSVPassword::ColumnMap kColMap = {
      {0, CSVPassword::Label::kOrigin},
      {1, CSVPassword::Label::kUsername},
      {2, CSVPassword::Label::kPassword},
  };
  constexpr base::StringPiece kCSVBlob =
      "http://example.com,u1,p1\n"
      "http://example.com,u2,p2\r\n"
      "http://example.com,u3,p\r\r\n"
      "http://example.com,\"u\n4\",\"p\n4\"\n"
      "http://example.com,u5,p5";
  constexpr base::StringPiece kExpectedPasswords[] = {"p1", "p2", "p\r", "p\n4",
                                                      "p5"};

  CSVPasswordIterator iter(kColMap, kCSVBlob);

  CSVPasswordIterator check = iter;
  for (size_t i = 0; i < base::size(kExpectedPasswords); ++i) {
    EXPECT_EQ(CSVPassword::Status::kOK, (check++)->Parse(nullptr))
        << "on line " << i;
  }
  EXPECT_NE(CSVPassword::Status::kOK, check->Parse(nullptr));

  for (const base::StringPiece& expected_password : kExpectedPasswords) {
    PasswordForm result = (iter++)->ParseValid();
    // Detailed checks of the parsed result are made in the test for
    // CSVPassword. Here only the last field (password) is checked to (1) ensure
    // that lines are processed in the expected sequence, and (2) line breaks
    // are handled as expected (in particular, '\r' alone is not a line break).
    EXPECT_EQ(base::ASCIIToUTF16(expected_password), result.password_value);
  }
}

TEST(CSVPasswordIteratorTest, Failure) {
  const CSVPassword::ColumnMap kColMap = {
      {0, CSVPassword::Label::kOrigin},
      {1, CSVPassword::Label::kUsername},
      {2, CSVPassword::Label::kPassword},
  };
  constexpr base::StringPiece kCSVBlob =
      "too few fields\n"
      "http://example.com,\"\"trailing,p\n"
      "http://notascii.ž.com,u,p\n"
      "http://example.com,empty-password,\n"
      "http://no-failure.example.com,to check that,operator++ worked";
  constexpr size_t kLinesInBlob = 5;

  CSVPasswordIterator iter(kColMap, kCSVBlob);

  CSVPasswordIterator check = iter;
  for (size_t i = 0; i + 1 < kLinesInBlob; ++i) {
    EXPECT_NE(CSVPassword::Status::kOK, (check++)->Parse(nullptr))
        << "on line " << i;
  }
  // Last line was not a failure.
  EXPECT_EQ(CSVPassword::Status::kOK, (check++)->Parse(nullptr));
  // After iterating over all lines, there is no more data to parse.
  EXPECT_NE(CSVPassword::Status::kOK, check->Parse(nullptr));
}

}  // namespace password_manager
