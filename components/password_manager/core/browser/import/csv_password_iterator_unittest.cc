// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password_iterator.h"

#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using Label = CSVPassword::Label;

TEST(CSVPasswordIteratorTest, Operations) {
  // Default.
  CSVPasswordIterator def;
  EXPECT_EQ(def, def);

  // From CSV.
  const CSVPassword::ColumnMap kColMap = {
      {0, Label::kOrigin},
      {1, Label::kUsername},
      {2, Label::kPassword},
  };
  constexpr base::StringPiece kCSV = "http://example.com,user,password";
  CSVPasswordIterator iter(kColMap, kCSV);
  // Because kCSV is just one row, it can be used to create a CSVPassword
  // directly.

  // Mock time so that date_created matches.
  {
    base::test::SingleThreadTaskEnvironment env(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    EXPECT_EQ(iter->ParseValid(), CSVPassword(kColMap, kCSV).ParseValid());
  }

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
  const CSVPassword::ColumnMap kColMapCopy = kColMap;
  CSVPasswordIterator same_looking(kColMapCopy, kCSV);
  EXPECT_NE(same_looking, iter);
  CSVPasswordIterator old = iter++;
  EXPECT_NE(old, iter);
  EXPECT_EQ(++old, iter);
}

TEST(CSVPasswordIteratorTest, MostRowsCorrect) {
  const CSVPassword::ColumnMap kColMap = {
      {1, Label::kOrigin},
      {4, Label::kUsername},
      {2, Label::kPassword},
  };
  constexpr base::StringPiece kCSVBlob =
      ",http://example.com,p1,.,u1\n"
      "something,http://example.com,p2,T,u2\r\n"
      // The empty line below is completely ignored.
      "\r\n"
      "other,http://example.com,p3,>,u\r\r\n"
      "***,http://example.com,\"p\n4\",\"\n\",\"u\n4\"\n"
      // The row below is also ignored, because it contains no valid URL.
      ",:,p,,u\n"
      "\",\",http://example.com,p5,;,u5\n"
      // The last row is ignored, because it is invalid.
      ",http://example.com,p,,u,\"\n";
  constexpr base::StringPiece kExpectedUsernames[] = {"u1", "u2", "u\r", "u\n4",
                                                      "u5"};

  CSVPasswordIterator iter(kColMap, kCSVBlob);

  CSVPasswordIterator check = iter;
  for (size_t i = 0; i < base::size(kExpectedUsernames); ++i) {
    EXPECT_EQ(CSVPassword::Status::kOK, (check++)->TryParse())
        << "on line " << i;
  }
  EXPECT_NE(CSVPassword::Status::kOK, check->TryParse());

  for (const base::StringPiece& expected_username : kExpectedUsernames) {
    PasswordForm result = (iter++)->ParseValid();
    // Detailed checks of the parsed result are made in the test for
    // CSVPassword. Here only the last field (username) is checked to (1) ensure
    // that lines are processed in the expected sequence, and (2) line breaks
    // are handled as expected (in particular, '\r' alone is not a line break).
    EXPECT_EQ(base::ASCIIToUTF16(expected_username), result.username_value);
  }
}

TEST(CSVPasswordIteratorTest, LastRowCorrect) {
  const CSVPassword::ColumnMap kColMap = {
      {0, Label::kOrigin},
      {1, Label::kUsername},
      {2, Label::kPassword},
  };
  constexpr base::StringPiece kCSVBlob =
      "too few fields\n"
      "http://example.com,\"\"trailing,p\n"
      "http://notascii.ž.com,u,p\n"
      "http://example.com,empty-password,\n"
      "http://no-failure.example.com,to check that,operator++ worked";

  CSVPasswordIterator iter(kColMap, kCSVBlob);

  PasswordForm pf;
  // The iterator should skip all the faulty rows and land on the last one.
  EXPECT_EQ(CSVPassword::Status::kOK, (iter++)->Parse(&pf));
  EXPECT_EQ("http://no-failure.example.com/", pf.signon_realm);

  // After iterating over all lines, there is no more data to parse.
  EXPECT_NE(CSVPassword::Status::kOK, iter->TryParse());
}

TEST(CSVPasswordIteratorTest, NoRowCorrect) {
  const CSVPassword::ColumnMap kColMap = {
      {0, Label::kOrigin},
      {1, Label::kUsername},
      {2, Label::kPassword},
  };
  constexpr base::StringPiece kCSVBlob =
      "too few fields\n"
      "http://example.com,\"\"trailing,p\n"
      "http://notascii.ž.com,u,p\n"
      "http://example.com,empty-password,";

  CSVPasswordIterator iter(kColMap, kCSVBlob);
  CSVPasswordIterator end(kColMap, base::StringPiece());

  EXPECT_EQ(iter, end);
}

}  // namespace password_manager
