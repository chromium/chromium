// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/header.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "components/domain_reliability/config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {
namespace {

class DomainReliabilityHeaderTest : public testing::Test {
 protected:
  DomainReliabilityHeaderTest() {}
  ~DomainReliabilityHeaderTest() override {}

  void Parse(base::StringPiece value) {
    // Run the parser over a non-NUL-terminated buffer, so ASan will catch
    // StringPiece misuses.
    std::unique_ptr<char[]> copy(new char[value.size()]);
    std::copy(value.begin(), value.end(), copy.get());
    parsed_ = DomainReliabilityHeader::Parse(
        base::StringPiece(copy.get(), value.size()));
  }

  const DomainReliabilityHeader* parsed() const { return parsed_.get(); }

 private:
  std::unique_ptr<DomainReliabilityHeader> parsed_;
};

bool CheckReportUris(
    const char* pipe_separated_expected_report_uris,
    const std::vector<std::unique_ptr<GURL>>& actual_report_uris) {
  if (!pipe_separated_expected_report_uris)
    return actual_report_uris.empty();

  std::vector<std::string> expected_report_uri_strings =
      SplitString(pipe_separated_expected_report_uris,
                  "|",
                  base::KEEP_WHITESPACE,
                  base::SPLIT_WANT_ALL);
  if (expected_report_uri_strings.size() != actual_report_uris.size())
    return false;

  for (size_t i = 0; i < actual_report_uris.size(); ++i) {
    if (actual_report_uris[i]->spec() != expected_report_uri_strings[i])
      return false;
  }

  return true;
}

TEST_F(DomainReliabilityHeaderTest, SetConfig) {
  static const struct {
    const char* header;
    const char* pipe_separated_report_uris;
    int64_t max_age_in_seconds;
    bool include_subdomains;
    const char* description;
  } test_cases[] = {
    { "report-uri=https://a/; max-age=5",
          "https://a/", 5, false,
          "register" },
    { "report-uri=\"https://a/\"; max-age=5",
          "https://a/", 5, false,
          "register with quoted report-uri" },
    { "report-uri=<https://a/>; max-age=5",
          "https://a/", 5, false,
          "register with bracketed report-uri" },
    { "report-uri=https://a/ https://b/; max-age=5",
          "https://a/|https://b/", 5, false,
          "register with two report-uris" },
    { "report-uri=https://a/; max-age=5; includeSubdomains",
          "https://a/", 5, true,
          "register with includeSubdomains" },
  };

  for (const auto& test_case : test_cases) {
    std::string assert_prefix = base::StringPrintf(
        "Valid set-config NEL header \"%s\" (%s) incorrectly parsed as ",
        test_case.header,
        test_case.description);
    Parse(test_case.header);
    EXPECT_FALSE(parsed()->IsParseError()) << assert_prefix << "invalid.";
    EXPECT_FALSE(parsed()->IsClearConfig()) << assert_prefix << "clear-config.";
    if (parsed()->IsParseError() || parsed()->IsClearConfig())
      continue;
    EXPECT_TRUE(parsed()->IsSetConfig());

    std::string assert_message =
        assert_prefix + "\"" + parsed()->ToString() + "\"";
    EXPECT_TRUE(CheckReportUris(test_case.pipe_separated_report_uris,
                                parsed()->config().collectors))
        << assert_message;
    EXPECT_EQ(test_case.max_age_in_seconds,
              parsed()->max_age().InSeconds())
        << assert_message;
    EXPECT_EQ(test_case.include_subdomains,
              parsed()->config().include_subdomains)
        << assert_message;
  }
}

TEST_F(DomainReliabilityHeaderTest, ClearConfig) {
  static const struct {
    const char* header;
    const char* description;
  } test_cases[] = {
    { "max-age=0", "unregister" },
    { "report-uri=https://a/; max-age=0", "unregister with report-uri" },
    { "max-age=0; includeSubdomains", "unregister with includeSubdomains" },
  };

  for (const auto& test_case : test_cases) {
    std::string assert_prefix = base::StringPrintf(
        "Valid clear-config NEL header \"%s\" (%s) incorrectly parsed as ",
        test_case.header,
        test_case.description);
    Parse(test_case.header);
    EXPECT_FALSE(parsed()->IsParseError()) << assert_prefix << "invalid.";
    EXPECT_FALSE(parsed()->IsSetConfig()) << assert_prefix << "set-config.";
  }
}

TEST_F(DomainReliabilityHeaderTest, Error) {
  static const struct {
    const char* header;
    const char* description;
  } test_cases[] = {
    { "", "empty" },
    { "max-age=5", "report-uri missing with non-zero max-age" },
    { "report-uri; max-age=5", "report-uri has no value" },
    { "report-uri=; max-age=5", "report-uri value is empty" },
    { "report-uri=http://a/; max-age=5", "report-uri is insecure" },
    { "report-uri=https://a/ http://b/; max-age=5",
          "one report-uri of two is insecure" },
    { "report-uri=\"https://a/; max-age=5", "report-uri is unbalanced" },
    { "report-uri=https://a/\"; max-age=5", "report-uri is unbalanced" },
    { "report-uri=<https://a/; max-age=5", "report-uri is unbalanced" },
    { "report-uri=https://a/>; max-age=5", "report-uri is unbalanced" },
    { "report-uri=https://a/", "max-age is missing" },
    { "report-uri=https://a/; max-age", "max-age has no value" },
    { "report-uri=https://a/; max-age=", "max-age value is empty" },
    { "report-uri=https://a/; max-age=a", "max-age is entirely non-numeric" },
    { "report-uri=https://a/; max-age=5a", "max-age is partly non-numeric" },
    { "report-uri=https://a/; max-age=5 5", "max-age has multiple values" },
  };

  for (const auto& test_case : test_cases) {
    Parse(test_case.header);
    EXPECT_TRUE(parsed()->IsParseError())
        << "Invalid NEL header \"" << test_case.header << "\" ("
        << test_case.description << ") incorrectly parsed as valid.";
  }
}

}  // namespace
}  // namespace domain_reliability
