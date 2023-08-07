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

  void ParseCSV(const std::string& raw_json,
                mojom::CSVPasswordParser::ParseCSVCallback callback) {
    parser_->ParseCSV(raw_json, std::move(callback));
  }

 private:
  std::unique_ptr<CSVPasswordParserImpl> parser_;
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

}  // namespace password_manager
