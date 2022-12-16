// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_password.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using Label = CSVPassword::Label;
using Status = CSVPassword::Status;

TEST(CSVPasswordTest, Construction) {
  base::test::SingleThreadTaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  const CSVPassword::ColumnMap kColMap = {
      {0, Label::kOrigin},
      {1, Label::kUsername},
      {2, Label::kPassword},
  };
  // Use const to check that ParseValid does not mutate the CSVPassword.
  const CSVPassword csv_pwd(kColMap, "http://example.com,user,password");
  EXPECT_EQ(GURL("http://example.com"), csv_pwd.GetURL());
  EXPECT_EQ("user", csv_pwd.GetUsername());
  EXPECT_EQ("password", csv_pwd.GetPassword());
}

struct TestCase {
  std::string name;
  // Input.
  CSVPassword::ColumnMap map;
  std::string csv;
  // Expected.
  std::string origin;
  std::string signon_realm;
  std::string username;
  std::string password;
  std::string note;
  Status status = Status::kOK;
};

class TestCaseBuilder {
 public:
  explicit TestCaseBuilder(std::string name) {
    test_case_.name = std::move(name);
  }

  ~TestCaseBuilder() = default;

  TestCaseBuilder& Map(CSVPassword::ColumnMap map) {
    test_case_.map = std::move(map);
    return *this;
  }

  TestCaseBuilder& CSV(std::string csv) {
    test_case_.csv = std::move(csv);
    return *this;
  }

  TestCaseBuilder& Origin(std::string origin) {
    test_case_.origin = std::move(origin);
    return *this;
  }

  TestCaseBuilder& SignonRealm(std::string signon_realm) {
    test_case_.signon_realm = std::move(signon_realm);
    return *this;
  }

  TestCaseBuilder& Username(std::string username) {
    test_case_.username = std::move(username);
    return *this;
  }

  TestCaseBuilder& Password(std::string password) {
    test_case_.password = std::move(password);
    return *this;
  }

  TestCaseBuilder& Note(std::string note) {
    test_case_.note = std::move(note);
    return *this;
  }

  TestCaseBuilder& Status(Status status) {
    test_case_.status = status;
    return *this;
  }

  TestCase Build() { return std::move(test_case_); }

 private:
  TestCase test_case_;
};

class CSVPasswordTestSuccess : public ::testing::TestWithParam<TestCase> {
 private:
  base::test::SingleThreadTaskEnvironment env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_P(CSVPasswordTestSuccess, ShouldParse) {
  const TestCase& test_case = GetParam();
  SCOPED_TRACE(test_case.name);
  const CSVPassword csv_pwd(test_case.map, test_case.csv);
  EXPECT_EQ(Status::kOK, csv_pwd.GetParseStatus());

  EXPECT_EQ(GURL(test_case.origin), csv_pwd.GetURL());
  EXPECT_EQ(test_case.username, csv_pwd.GetUsername());
  EXPECT_EQ(test_case.password, csv_pwd.GetPassword());
  EXPECT_EQ(test_case.note, csv_pwd.GetNote());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CSVPasswordTestSuccess,
    ::testing::Values(
        TestCaseBuilder("all columns specified")
            .Map({
                {0, Label::kOrigin},
                {1, Label::kUsername},
                {2, Label::kPassword},
                {3, Label::KNote},
            })
            .CSV("http://example.com,user,password,secret note")
            .Origin("http://example.com")
            .SignonRealm("http://example.com/")
            .Username("user")
            .Password("password")
            .Note("secret note")
            .Build(),
        TestCaseBuilder("empty username")
            .Map({{0, Label::kOrigin},
                  {1, Label::kPassword},
                  {2, Label::kUsername}})
            .CSV("http://example.com,password,")
            .Origin("http://example.com")
            .SignonRealm("http://example.com/")
            .Username("")
            .Password("password")
            .Build(),
        TestCaseBuilder("permuted and inserted columns")
            .Map({{2, Label::kOrigin},
                  {1, Label::kUsername},
                  {4, Label::kPassword}})
            .CSV("X,the-user,https://example.org,Y,pwd")
            .Origin("https://example.org")
            .SignonRealm("https://example.org/")
            .Username("the-user")
            .Password("pwd")
            .Build(),
        TestCaseBuilder("Android")
            .Map({{2, Label::kOrigin},
                  {1, Label::kUsername},
                  {0, Label::kPassword}})
            .CSV("pwd,the-user,android://host@example,Y,X")
            .Origin("android://host@example")
            .SignonRealm("android://host@example")
            .Username("the-user")
            .Password("pwd")
            .Build(),
        TestCaseBuilder("Escaped quotes")
            .Map({{0, Label::kOrigin},
                  {1, Label::kUsername},
                  {2, Label::kPassword}})
            .CSV(R"(http://example.org,"","""""")")
            .Origin("http://example.org")
            .SignonRealm("http://example.org/")
            .Username("")
            .Password("\"\"")
            .Build(),
        TestCaseBuilder("Escaped quotes 2")
            .Map({{0, Label::kOrigin},
                  {1, Label::kUsername},
                  {2, Label::kPassword}})
            .CSV(R"(http://example.org,"""","A""B""""C")")
            .Origin("http://example.org")
            .SignonRealm("http://example.org/")
            .Username("\"")
            .Password("A\"B\"\"C")
            .Build(),
        TestCaseBuilder("Escaped commas and quotes")
            .Map({{0, Label::kOrigin},
                  {2, Label::kUsername},
                  {3, Label::kPassword}})
            .CSV(R"(http://example.org,"a""b","u,+,c","p""")")
            .Origin("http://example.org")
            .SignonRealm("http://example.org/")
            .Username("u,+,c")
            .Password("p\"")
            .Build(),
        TestCaseBuilder("Escaped line breaks")
            .Map({{0, Label::kOrigin},
                  {2, Label::kUsername},
                  {1, Label::kPassword}})
            .CSV("http://example.org,\"a\nb\",\"u\r\nse\rr\"")
            .Origin("http://example.org")
            .SignonRealm("http://example.org/")
            .Username("u\r\nse\rr")
            .Password("a\nb")
            .Build(),
        TestCaseBuilder("Extra spaces preserved")
            .Map({{0, Label::kOrigin},
                  {1, Label::kUsername},
                  {2, Label::kPassword}})
            .CSV("http://example.org, user ,  pwd  ")
            .Origin("http://example.org")
            .SignonRealm("http://example.org/")
            .Username(" user ")
            .Password("  pwd  ")
            .Build(),
        TestCaseBuilder("Enclosing quotes are trimmed")
            .Map({{0, Label::kOrigin},
                  {1, Label::kUsername},
                  {2, Label::kPassword}})
            .CSV("http://example.org,\"user\",\"pwd\"")
            .Origin("http://example.org")
            .SignonRealm("http://example.org/")
            .Username("user")
            .Password("pwd")
            .Build(),
        TestCaseBuilder("Chars outside ASCII printable preserved verbatim")
            .Map({{0, Label::kOrigin},
                  {1, Label::kUsername},
                  {2, Label::kPassword}})
            .CSV("http://example.org,\x07\t\x0B\x1F,"
                 "$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2")
            .Origin("http://example.org")
            .SignonRealm("http://example.org/")
            // Characters below 0x20: bell, horizontal + vertical tab, unit
            // separator.
            .Username("\x07\t\x0B\x1F")
            // Unicode code points having 1..4 byte UTF-8 representation:
            // dollar sign (U+0024), cent sign (U+00A2), snowman (U+2603), Han
            // character U+24B62.
            .Password("$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2")
            .Build(),
        TestCaseBuilder("path discarded")
            .Map({{2, Label::kOrigin},
                  {1, Label::kUsername},
                  {0, Label::kPassword}})
            .CSV("password,user,http://example.com/path")
            .Origin("http://example.com/path")
            .SignonRealm("http://example.com/")
            .Username("user")
            .Password("password")
            .Build()));

class CSVPasswordTestFailure : public ::testing::TestWithParam<TestCase> {};

TEST_P(CSVPasswordTestFailure, ShouldFailWithStatus) {
  const TestCase& test_case = GetParam();
  SCOPED_TRACE(test_case.name);
  EXPECT_EQ(test_case.status,
            CSVPassword(test_case.map, test_case.csv).GetParseStatus());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CSVPasswordTestFailure,
    ::testing::Values(TestCaseBuilder("empty line")
                          .Map({})
                          .CSV("")
                          .Status(Status::kSemanticError)
                          .Build(),
                      TestCaseBuilder("malformed CSV")
                          .Map({{0, Label::kOrigin},
                                {1, Label::kUsername},
                                {2, Label::kPassword}})
                          .CSV("\"")
                          .Status(Status::kSyntaxError)
                          .Build(),
                      TestCaseBuilder("another malformed CSV")
                          .Map({{0, Label::kOrigin},
                                {1, Label::kUsername},
                                {2, Label::kPassword}})
                          .CSV("Url,Username,\"Password\n")
                          .Status(Status::kSyntaxError)
                          .Build()));

}  // namespace password_manager
