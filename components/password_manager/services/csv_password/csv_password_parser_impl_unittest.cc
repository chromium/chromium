// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/csv_password_parser_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::IsEmpty;

namespace password_manager {

class CSVPasswordParserImplTest : public testing::Test {
 protected:
  CSVPasswordParserImplTest() {
    mojo::PendingReceiver<mojom::CSVPasswordParser> receiver;
    parser_ = std::make_unique<CSVPasswordParserImpl>(std::move(receiver));
  }

  void ParseCSV(const std::string& raw_csv,
                mojom::CSVPasswordParser::ParseCSVCallback callback) {
    parser_->ParseCSV(raw_csv, std::move(callback));
  }

 private:
  std::unique_ptr<CSVPasswordParserImpl> parser_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CSVPasswordParserImplTest, ParseEmptyFile) {
  ParseCSV("", base::BindLambdaForTesting(
                   [](mojom::CSVPasswordSequencePtr sequence) {
                     EXPECT_FALSE(sequence);
                   }));
}

TEST_F(CSVPasswordParserImplTest, ParseHeaderOnlyFile) {
  const std::string raw_csv =
      R"(Display Name,   ,Login,Secret Question,Password,URL,Timestamp)";

  ParseCSV(raw_csv, base::BindLambdaForTesting(
                        [](mojom::CSVPasswordSequencePtr sequence) {
                          EXPECT_TRUE(sequence);
                          EXPECT_THAT(sequence->csv_passwords, IsEmpty());
                        }));
}

TEST_F(CSVPasswordParserImplTest, ParseHeaderOnlyFileOnlyBasics) {
  const std::string raw_csv = R"(Login,Password,URL)";

  ParseCSV(raw_csv, base::BindLambdaForTesting(
                        [](mojom::CSVPasswordSequencePtr sequence) {
                          EXPECT_TRUE(sequence);
                          EXPECT_THAT(sequence->csv_passwords, IsEmpty());
                        }));
}

TEST_F(CSVPasswordParserImplTest, ParseGoodFile) {
  const std::string raw_csv =
      R"(Display Name,   ,Login,Secret Question,Password,URL,Timestamp
        DN           , v ,user,?               ,pwd,http://example.com,123
                     , < ,Alice,123?           ,even,https://example.net,213,pas
        :)           ,   ,Bob,ABCD!            ,odd,https://example.org,132)";

  ParseCSV(
      raw_csv,
      base::BindLambdaForTesting([&](mojom::CSVPasswordSequencePtr sequence) {
        EXPECT_TRUE(sequence);
        EXPECT_EQ(3u, sequence->csv_passwords.size());

        EXPECT_EQ(GURL("http://example.com"),
                  sequence->csv_passwords[0].GetURL());
        EXPECT_EQ("user", sequence->csv_passwords[0].GetUsername());
        EXPECT_EQ("pwd", sequence->csv_passwords[0].GetPassword());

        EXPECT_EQ(GURL("https://example.net"),
                  sequence->csv_passwords[1].GetURL());
        EXPECT_EQ("Alice", sequence->csv_passwords[1].GetUsername());
        EXPECT_EQ("even", sequence->csv_passwords[1].GetPassword());

        EXPECT_EQ(GURL("https://example.org"),
                  sequence->csv_passwords[2].GetURL());
        EXPECT_EQ("Bob", sequence->csv_passwords[2].GetUsername());
        EXPECT_EQ("odd", sequence->csv_passwords[2].GetPassword());
      }));
}

TEST_F(CSVPasswordParserImplTest, ParseFileMissingPassword) {
  const std::string raw_csv =
      R"(Display Name,   ,Login,Secret Question,Password,URL,Timestamp
        :)           ,   ,Bob,ABCD!            ,,https://example.org,132)";

  ParseCSV(raw_csv, base::BindLambdaForTesting(
                        [&](mojom::CSVPasswordSequencePtr sequence) {
                          EXPECT_TRUE(sequence);
                          EXPECT_EQ(1u, sequence->csv_passwords.size());
                          EXPECT_EQ("Bob",
                                    sequence->csv_passwords[0].GetUsername());
                          EXPECT_EQ(GURL("https://example.org"),
                                    sequence->csv_passwords[0].GetURL());
                        }));
}

TEST_F(CSVPasswordParserImplTest, ParseFileMissingFields) {
  const std::string raw_csv =
      R"(Display Name,   ,Login,Secret Question,Password,URL,Timestamp
        :)        Bob,ABCD!,blabla,https://example.org,132)";

  ParseCSV(
      raw_csv,
      base::BindLambdaForTesting([&](mojom::CSVPasswordSequencePtr sequence) {
        EXPECT_TRUE(sequence);
        EXPECT_EQ(1u, sequence->csv_passwords.size());
        EXPECT_EQ("blabla", sequence->csv_passwords[0].GetUsername());
        EXPECT_EQ("132", sequence->csv_passwords[0].GetPassword());
        EXPECT_THAT(sequence->csv_passwords[0].GetURL(),
                    base::test::ErrorIs(""));
      }));
}

TEST_F(CSVPasswordParserImplTest, SkipsEmptyLines) {
  const std::string csv =
      "Display Name,Login,Secret Question,Password,URL,Timestamp\n"
      "\n"
      "\t\t\t\r\n"
      "            \n"
      "non_empty,pwd\n"
      "non_empty,pwd\n"
      "    ";

  ParseCSV(
      csv,
      base::BindLambdaForTesting([&](mojom::CSVPasswordSequencePtr sequence) {
        EXPECT_TRUE(sequence);
        EXPECT_EQ(2u, sequence->csv_passwords.size());
        EXPECT_EQ("pwd", sequence->csv_passwords[0].GetUsername());
        EXPECT_EQ("pwd", sequence->csv_passwords[1].GetUsername());
      }));
}

TEST_F(CSVPasswordParserImplTest, Iteration) {
  const std::string csv =
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

  ParseCSV(csv, base::BindLambdaForTesting(
                    [&](mojom::CSVPasswordSequencePtr sequence) {
                      EXPECT_TRUE(sequence);
                      EXPECT_EQ(3u, sequence->csv_passwords.size());

                      size_t order = 0;
                      for (const CSVPassword& pwd : sequence->csv_passwords) {
                        ASSERT_LT(order, std::size(kExpectedCredentials));
                        const auto& expected = kExpectedCredentials[order];
                        EXPECT_EQ(GURL(expected.url), pwd.GetURL());
                        EXPECT_EQ(expected.username, pwd.GetUsername());
                        EXPECT_EQ(expected.password, pwd.GetPassword());
                        EXPECT_EQ(expected.note, pwd.GetNote());
                        ++order;
                      }
                    }));
}

TEST_F(CSVPasswordParserImplTest, MissingEolAtEof) {
  const std::string csv = "url,login,password\nhttp://a.com,l,p";

  ParseCSV(
      csv,
      base::BindLambdaForTesting([&](mojom::CSVPasswordSequencePtr sequence) {
        EXPECT_TRUE(sequence);
        EXPECT_EQ(1u, sequence->csv_passwords.size());

        EXPECT_EQ(GURL("http://a.com"), sequence->csv_passwords[0].GetURL());
        EXPECT_EQ("l", sequence->csv_passwords[0].GetUsername());
        EXPECT_EQ("p", sequence->csv_passwords[0].GetPassword());
      }));
}

TEST_F(CSVPasswordParserImplTest, AcceptsDifferentNoteColumnNames) {
  const std::string note_column_names[] = {"note", "notes", "comment",
                                           "comments"};
  for (auto const& note_column_name : note_column_names) {
    const std::string csv =
        "url,login,password," + note_column_name + "\nhttp://a.com,l,p,n";

    ParseCSV(csv, base::BindLambdaForTesting(
                      [&](mojom::CSVPasswordSequencePtr sequence) {
                        EXPECT_TRUE(sequence);
                        EXPECT_EQ(1u, sequence->csv_passwords.size());

                        EXPECT_EQ("n", sequence->csv_passwords[0].GetNote());
                      }));
  }
}

TEST_F(CSVPasswordParserImplTest, ContainsMultipleAcceptableNoteColumns) {
  // In such cases note column names priority should be taken into account.
  // note > notes > comment > comments
  const std::string csv =
      "url,login,password,comment,note,notes\n"
      "http://a.com,l,p,note a,note b,note c";

  ParseCSV(csv, base::BindLambdaForTesting(
                    [&](mojom::CSVPasswordSequencePtr sequence) {
                      EXPECT_TRUE(sequence);
                      EXPECT_EQ(1u, sequence->csv_passwords.size());

                      EXPECT_EQ("note b", sequence->csv_passwords[0].GetNote());
                    }));
}

TEST_F(CSVPasswordParserImplTest, NonASCIICharacters) {
  const std::string csv =
      "username,password,note,origin\n"
      "\x07\t\x0B\x1F,$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2,\"A\"\n";

  ParseCSV(
      csv,
      base::BindLambdaForTesting([&](mojom::CSVPasswordSequencePtr sequence) {
        EXPECT_TRUE(sequence);
        EXPECT_EQ(1u, sequence->csv_passwords.size());

        EXPECT_EQ("\x07\t\x0B\x1F", sequence->csv_passwords[0].GetUsername());
        EXPECT_EQ("$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2",
                  sequence->csv_passwords[0].GetPassword());
        EXPECT_EQ("A", sequence->csv_passwords[0].GetNote());
      }));
}

TEST_F(CSVPasswordParserImplTest, EscapedCharacters) {
  const std::string csv =
      "username, password, note, origin\n"
      "\"A\rB\",\"B\nC\",\"C\r\nD\",\"D\n";

  ParseCSV(
      csv,
      base::BindLambdaForTesting([&](mojom::CSVPasswordSequencePtr sequence) {
        EXPECT_TRUE(sequence);
        EXPECT_EQ(1u, sequence->csv_passwords.size());

        EXPECT_EQ("A\rB", sequence->csv_passwords[0].GetUsername());
        EXPECT_EQ("B\nC", sequence->csv_passwords[0].GetPassword());
        EXPECT_EQ("C\r\nD", sequence->csv_passwords[0].GetNote());
      }));
}

}  // namespace password_manager
