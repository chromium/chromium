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

struct RowTestCase {
  std::string name;
  std::string csv;
  std::string origin;
  std::string username;
  std::string password;
  std::string note;
};

using CSVPasswordSequenceRowSuccessTest = ::testing::TestWithParam<RowTestCase>;

TEST_P(CSVPasswordSequenceRowSuccessTest, ShouldParseRow) {
  const RowTestCase& test_case = GetParam();
  SCOPED_TRACE(test_case.name);

  ASSERT_OK_AND_ASSIGN(CSVPasswordSequence seq,
                       CSVPasswordSequence::Create(test_case.csv));
  std::optional<CSVPassword> csv_pwd = seq.NextPassword();
  ASSERT_TRUE(csv_pwd.has_value());
  EXPECT_EQ(CSVPassword::Status::kOK, csv_pwd->GetParseStatus());
  EXPECT_EQ(GURL(test_case.origin), csv_pwd->GetURL());
  EXPECT_EQ(test_case.username, csv_pwd->GetUsername());
  EXPECT_EQ(test_case.password, csv_pwd->GetPassword());
  EXPECT_EQ(test_case.note, csv_pwd->GetNote());
  EXPECT_EQ(seq.NextPassword(), std::nullopt);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CSVPasswordSequenceRowSuccessTest,
    ::testing::Values(
        RowTestCase{
            .name = "all columns specified",
            .csv = "url,username,password,note\n"
                   "http://example.com,user,password,secret note",
            .origin = "http://example.com",
            .username = "user",
            .password = "password",
            .note = "secret note",
        },
        RowTestCase{
            .name = "empty username",
            .csv = "url,password,username\n"
                   "http://example.com,password,",
            .origin = "http://example.com",
            .username = "",
            .password = "password",
        },
        RowTestCase{
            .name = "permuted and inserted columns",
            .csv = "a,username,url,b,password\n"
                   "X,the-user,https://example.org,Y,pwd",
            .origin = "https://example.org",
            .username = "the-user",
            .password = "pwd",
        },
        RowTestCase{
            .name = "Android",
            .csv = "password,username,url\n"
                   "pwd,the-user,android://host@example,Y,X",
            .origin = "android://host@example",
            .username = "the-user",
            .password = "pwd",
        },
        RowTestCase{
            .name = "Escaped quotes",
            .csv = "url,username,password\n"
                   R"(http://example.org,"","""""")",
            .origin = "http://example.org",
            .username = "",
            .password = "\"\"",
        },
        RowTestCase{
            .name = "Escaped quotes 2",
            .csv = "url,username,password\n"
                   R"(http://example.org,"""","A""B""""C")",
            .origin = "http://example.org",
            .username = "\"",
            .password = "A\"B\"\"C",
        },
        RowTestCase{
            .name = "Escaped commas and quotes",
            .csv = "url,a,username,password\n"
                   R"(http://example.org,"a""b","u,+,c","p""")",
            .origin = "http://example.org",
            .username = "u,+,c",
            .password = "p\"",
        },
        RowTestCase{
            .name = "Escaped line breaks",
            .csv = "url,password,username\n"
                   "http://example.org,\"a\nb\",\"u\r\nse\rr\"",
            .origin = "http://example.org",
            .username = "u\r\nse\rr",
            .password = "a\nb",
        },
        RowTestCase{
            .name = "Extra spaces preserved",
            .csv = "url,username,password\n"
                   "http://example.org, user ,  pwd  ",
            .origin = "http://example.org",
            .username = " user ",
            .password = "  pwd  ",
        },
        RowTestCase{
            .name = "Enclosing quotes are trimmed",
            .csv = "url,username,password\n"
                   "http://example.org,\"user\",\"pwd\"",
            .origin = "http://example.org",
            .username = "user",
            .password = "pwd",
        },
        RowTestCase{
            .name = "Chars outside ASCII printable preserved verbatim",
            // Characters below 0x20: bell, horizontal + vertical tab, unit
            // separator. Unicode code points having 1..4 byte UTF-8
            // representation: dollar sign (U+0024), cent sign (U+00A2),
            // snowman (U+2603), Han character U+24B62.
            .csv = "url,username,password\n"
                   "http://example.org,\x07\t\x0B\x1F,"
                   "$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2",
            .origin = "http://example.org",
            .username = "\x07\t\x0B\x1F",
            .password = "$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2",
        },
        RowTestCase{
            .name = "path preserved",
            .csv = "password,username,url\n"
                   "password,user,http://example.com/path",
            .origin = "http://example.com/path",
            .username = "user",
            .password = "password",
        }));

TEST(CSVPasswordSequenceTest, RowSyntaxError) {
  for (std::string_view malformed_row : {"\"", "Url,Username,\"Password\n"}) {
    std::string csv =
        std::string("url,username,password\n") + std::string(malformed_row);
    ASSERT_OK_AND_ASSIGN(CSVPasswordSequence seq,
                         CSVPasswordSequence::Create(csv));
    std::optional<CSVPassword> csv_pwd = seq.NextPassword();
    ASSERT_TRUE(csv_pwd.has_value());
    EXPECT_EQ(CSVPassword::Status::kSyntaxError, csv_pwd->GetParseStatus());
  }
}

}  // namespace password_manager
