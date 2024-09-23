// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password_iterator.h"

#include <string>
#include <string_view>
#include <utility>

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
  constexpr std::string_view kCSV = "http://example.com,user,password";
  CSVPasswordIterator iter(kColMap, kCSV);
  // Because kCSV is just one row, it can be used to create a CSVPassword
  // directly.

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

TEST(CSVPasswordIteratorTest, MostRowsAreValid) {
  const CSVPassword::ColumnMap kColMap = {
      {0, Label::kOrigin},
      {1, Label::kUsername},
      {2, Label::kPassword},
  };
  constexpr std::string_view kCSVBlob =
      "\r\n"
      "\t\t\t\n"
      "http://no-failure.example.com,user_1,pwd]\n"
      "\n"
      "http://no-failure.example.com,user_2,pwd]\n"
      "http://no-failure.example.com,user_3,pwd]\n"
      "http://no-failure.example.com,user_4,pwd]\n";

  CSVPasswordIterator iter(kColMap, kCSVBlob);

  // The iterator should skip all the empty rows and land on the first valid
  // one.

  for (size_t i = 0; i < 4; i++) {
    EXPECT_EQ(CSVPassword::Status::kOK, iter->GetParseStatus());
    iter++;
  }

  // After iterating over all lines, there is no more data to parse.
  EXPECT_NE(CSVPassword::Status::kOK, iter->GetParseStatus());
}

TEST(CSVPasswordIteratorTest, LastRowNonEmpty) {
  const CSVPassword::ColumnMap kColMap = {
      {0, Label::kOrigin},
      {1, Label::kUsername},
      {2, Label::kPassword},
  };
  constexpr std::string_view kCSVBlob =
      "        \n"
      " \n"
      "\n"
      "http://no-failure.example.com,to check that,operator++ worked";

  CSVPasswordIterator iter(kColMap, kCSVBlob);

  // The iterator should skip all the faulty rows and land on the last one.
  EXPECT_EQ(CSVPassword::Status::kOK, iter->GetParseStatus());
  EXPECT_EQ("http://no-failure.example.com/", iter->GetURL());

  iter++;
  // After iterating over all lines, there is no more data to parse.
  EXPECT_NE(CSVPassword::Status::kOK, iter->GetParseStatus());
}

TEST(CSVPasswordIteratorTest, AllRowsAreEmpty) {
  const CSVPassword::ColumnMap kColMap = {
      {0, Label::kOrigin},
      {1, Label::kUsername},
      {2, Label::kPassword},
  };
  constexpr std::string_view kCSVBlob =
      "     \t   \r\n"
      " \n"
      "\n"
      "         ";

  CSVPasswordIterator iter(kColMap, kCSVBlob);
  CSVPasswordIterator end(kColMap, std::string_view());
  EXPECT_EQ(0, std::distance(iter, end));
  EXPECT_EQ(iter, end);
}

}  // namespace password_manager
