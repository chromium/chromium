// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password_sequence.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

using ::base::test::ErrorIs;

TEST(CSVPasswordSequenceTest, SkipsEmptyLines) {
  static constexpr char kNoUrl[] =
      "Display Name,Login,Secret Question,Password,URL,Timestamp\n"
      "\n"
      "\t\t\t\r\n"
      "            \n"
      "non_empty,pwd\n"
      "non_empty,pwd\n"
      "    ";
  ASSERT_OK_AND_ASSIGN(CSVPasswordSequence seq,
                       CSVPasswordSequence::Create(kNoUrl));
  EXPECT_NE(seq.NextPassword(), std::nullopt);
  EXPECT_NE(seq.NextPassword(), std::nullopt);
  EXPECT_EQ(seq.NextPassword(), std::nullopt);
}

TEST(CSVPasswordSequenceTest, AllRowsAreEmpty) {
  static constexpr char kCsv[] =
      "url,username,password\n"
      "     \t   \r\n"
      " \n"
      "\n"
      "         ";
  ASSERT_OK_AND_ASSIGN(CSVPasswordSequence seq,
                       CSVPasswordSequence::Create(kCsv));
  EXPECT_EQ(seq.NextPassword(), std::nullopt);
}

TEST(CSVPasswordSequenceTest, Iteration) {
  static constexpr char kCsv[] =
      "Display Name,,Login,Secret Question,Password,URL,Timestamp,Note\n"
      "DN,value-of-an-empty-named-column,user,?,pwd,http://"
      "example.com,123,\"Note\nwith two lines\"\n"
      ",<,Alice,123?,even,https://example.net,213,,past header count = "
      "ignored\n"
      ":),,Bob,ABCD!,odd,https://example.org,132,regular note\n";
  struct ExpectedCredential {
    std::string_view url;
    std::string_view username;
    std::string_view password;
    std::string_view note;
  };
  constexpr auto kExpectedCredentials = std::to_array<ExpectedCredential>(
      {{"http://example.com", "user", "pwd", "Note\nwith two lines"},
       {"https://example.net", "Alice", "even", ""},
       {"https://example.org", "Bob", "odd", "regular note"}});
  ASSERT_OK_AND_ASSIGN(CSVPasswordSequence seq,
                       CSVPasswordSequence::Create(kCsv));

  for (const auto& expected : kExpectedCredentials) {
    std::optional<CSVPassword> pwd = seq.NextPassword();
    ASSERT_TRUE(pwd.has_value());
    EXPECT_EQ(GURL(expected.url), pwd->GetURL());
    EXPECT_EQ(expected.username, pwd->GetUsername());
    EXPECT_EQ(expected.password, pwd->GetPassword());
    EXPECT_EQ(expected.note, pwd->GetNote());
  }
  EXPECT_EQ(seq.NextPassword(), std::nullopt);
}

TEST(CSVPasswordSequenceTest, MissingEolAtEof) {
  static constexpr char kCsv[] =
      "url,login,password\n"
      "http://a.com,l,p";
  ASSERT_OK_AND_ASSIGN(CSVPasswordSequence seq,
                       CSVPasswordSequence::Create(kCsv));

  std::optional<CSVPassword> pwd = seq.NextPassword();
  ASSERT_TRUE(pwd.has_value());
  EXPECT_EQ(GURL("http://a.com"), pwd->GetURL());
  EXPECT_EQ("l", pwd->GetUsername());
  EXPECT_EQ("p", pwd->GetPassword());
  EXPECT_EQ(seq.NextPassword(), std::nullopt);
}

TEST(CSVPasswordSequenceTest, AcceptsDifferentNoteColumnNames) {
  const std::string note_column_names[] = {"note", "notes", "comment",
                                           "comments"};
  for (auto const& note_column_name : note_column_names) {
    const std::string kCsv =
        "url,login,password," + note_column_name + "\nhttp://a.com,l,p,n";

    ASSERT_OK_AND_ASSIGN(CSVPasswordSequence seq,
                         CSVPasswordSequence::Create(kCsv));
    std::optional<CSVPassword> pwd = seq.NextPassword();
    ASSERT_TRUE(pwd.has_value());
    EXPECT_EQ("n", pwd->GetNote());
    EXPECT_EQ(seq.NextPassword(), std::nullopt);
  }
}

TEST(CSVPasswordSequenceTest, ContainsMultipleAcceptableNoteColumns) {
  // In such cases note column names priority should be taken into account.
  // note > notes > comment > comments
  static constexpr char kCsv[] =
      "url,login,password,comment,note,notes\n"
      "http://a.com,l,p,note a,note b,note c";

  ASSERT_OK_AND_ASSIGN(CSVPasswordSequence seq,
                       CSVPasswordSequence::Create(kCsv));
  std::optional<CSVPassword> pwd = seq.NextPassword();
  ASSERT_TRUE(pwd.has_value());
  EXPECT_EQ("note b", pwd->GetNote());
  EXPECT_EQ(seq.NextPassword(), std::nullopt);
}

struct HeaderTestCase {
  std::string test_name;
  // Input.
  std::string csv;
  // Expected.
  CSVPassword::Status status;
};

using CSVPasswordSequenceCompatibilityTest =
    ::testing::TestWithParam<HeaderTestCase>;

TEST_P(CSVPasswordSequenceCompatibilityTest, HeaderParsedWithStatus) {
  const HeaderTestCase& test_case = GetParam();
  SCOPED_TRACE(test_case.test_name);

  auto seq = CSVPasswordSequence::Create(test_case.csv);
  if (test_case.status == CSVPassword::Status::kOK) {
    ASSERT_TRUE(seq.has_value());
    EXPECT_EQ(seq->NextPassword(), std::nullopt);
  } else {
    EXPECT_THAT(seq, ErrorIs(test_case.status));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CSVPasswordSequenceCompatibilityTest,
    testing::ValuesIn<HeaderTestCase>(
        {{"Default", "url,username,password", CSVPassword::Status::kOK},
         {"Uppercase", "URL,USERNAME,PASSWORD", CSVPassword::Status::kOK},
         {"Quoted", "\"url\",\"username\",\"password\"",
          CSVPassword::Status::kOK},
         {"Variation #1", "website,user,password", CSVPassword::Status::kOK},
         {"Variation #2", "origin,login,password", CSVPassword::Status::kOK},
         {"Variation #3", "hostname,account,password",
          CSVPassword::Status::kOK},
         {"Variation #4", "login_uri,login_username,login_password",
          CSVPassword::Status::kOK},
         {"Allow spaces in header",
          " Display Name ,  Login,Secret Question ,  Password,  URL,  "
          "Timestamp   ",
          CSVPassword::Status::kOK},
         // Leave out URL but use both "UserName" and "Login". That way the
         // username column is duplicated while the overall number of
         // interesting columns matches the number of labels.
         {"Duplicated columns",
          "UserName,Login,Secret Question,Password,Timestamp\n",
          CSVPassword::Status::kSemanticError},
         {"Missing columns", ":),Bob,ABCD!,odd,https://example.org,132\n",
          CSVPassword::Status::kSemanticError},
         {"Empty", "", CSVPassword::Status::kSyntaxError},
         {"Unmatched quote",
          "Display Name,Login,Secret Question,Password,URL,Timestamp,\"\n",
          CSVPassword::Status::kSyntaxError}}));

}  // namespace password_manager
