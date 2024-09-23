// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/legal_message_line.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Prints out a legal message |line| to |os|.
std::ostream& operator<<(std::ostream& os, const LegalMessageLine& line) {
  os << "{text: '" << line.text() << "', links: [";
  for (const LegalMessageLine::Link& link : line.links()) {
    os << "{range: (" << link.range.start() << ", " << link.range.end()
       << "), url: '" << link.url << "'}";
  }
  os << "]}";
  return os;
}

// Prints out legal message |lines| to |os|.
std::ostream& operator<<(std::ostream& os, const LegalMessageLines& lines) {
  os << "[";
  for (const LegalMessageLine& line : lines) {
    os << line;
  }
  os << "]";
  return os;
}

namespace {

using Link = LegalMessageLine::Link;

// A test case.
struct TestCase {
  std::string message_json;
  LegalMessageLines expected_lines;
  bool escape_apostrophes;
};

// Test case values are indices into the TestCaseData vector. This is done to
// avoid static initializers on types used by the TestCase data.
const std::vector<int>& TestCaseValues() {
  static base::NoDestructor<std::vector<int>> cases{
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};
  return *cases;
}

const std::vector<TestCase>& TestCaseData() {
  static base::NoDestructor<std::vector<TestCase>> cases{{
      TestCase{"{"
               "  \"line\" : [ {"
               "     \"template\": \"This is the entire message.\""
               "  } ]"
               "}",
               {TestLegalMessageLine("This is the entire message.")}},
      TestCase{"{"
               "  \"line\" : [ {"
               "     \"template\": \"Panda {0}.\","
               "     \"template_parameter\": [ {"
               "        \"display_text\": \"bears are fuzzy\","
               "        \"url\": \"http://www.example.com\""
               "     } ]"
               "  } ]"
               "}",
               {TestLegalMessageLine{"Panda bears are fuzzy.",
                                     {Link(6, 21, "http://www.example.com")}}}},
      // Legal message is invalid, so lines should be empty.
      TestCase{"{"
               "  \"line\" : [ {"
               "     \"template\": \"Panda {0}.\","
               "     \"template_parameter\": [ {"
               "        \"display_text\": \"bear\""
               "     } ]"
               "  } ]"
               "}",
               LegalMessageLines()},
      TestCase{"{"
               "  \"line\" : [ {"
               "    \"template\": \"Panda {0}.\","
               "    \"template_parameter\": [ {"
               "      \"url\": \"http://www.example.com\""
               "     } ]"
               "  } ]"
               "}",
               // Legal message is invalid, so lines should be empty.
               LegalMessageLines()},
      TestCase{"{"
               "  \"line\" : [ {"
               "    \"template\": \"Panda '{'{0}'}' '{1}' don't $1.\","
               "    \"template_parameter\": [ {"
               "      \"display_text\": \"bears\","
               "      \"url\": \"http://www.example.com\""
               "     } ]"
               "  } ]"
               "}",
               {TestLegalMessageLine("Panda {bears} {1} don't $1.",
                                     {Link(7, 12, "http://www.example.com")})}},
      // Consecutive dollar signs do not expand correctly (see comment in
      // ReplaceTemplatePlaceholders() in legal_message_line.cc). If this is
      // fixed and this test starts to fail, please update the "Caveats"
      // section of the LegalMessageLine::Parse() header file comment.
      TestCase{"{"
               "  \"line\" : [ {"
               "     \"template\": \"$$\""
               "  } ]"
               "}",
               {TestLegalMessageLine("$$$")}},
      // "${" does not expand correctly (see comment in
      // ReplaceTemplatePlaceholders() in legal_message_line.cc). If this is
      // fixed and this test starts to fail, please update the "Caveats"
      // section of the LegalMessageLine::Parse() header file comment.
      TestCase{"{"
               "  \"line\" : [ {"
               "    \"template\": \"${0}\","
               "    \"template_parameter\": [ {"
               "      \"display_text\": \"bears\","
               "      \"url\": \"http://www.example.com\""
               "    } ]"
               "  } ]"
               "}",
               LegalMessageLines()},
      TestCase{
          "{"
          "  \"line\" : [ {"
          "    \"template\": \"Panda {0} like {2} eat {1}.\","
          "    \"template_parameter\": [ {"
          "      \"display_text\": \"bears\","
          "      \"url\": \"http://www.example.com/0\""
          "    }, {"
          "      \"display_text\": \"bamboo\","
          "      \"url\": \"http://www.example.com/1\""
          "    }, {"
          "      \"display_text\": \"to\","
          "      \"url\": \"http://www.example.com/2\""
          "    } ]"
          "  } ]"
          "}",
          {TestLegalMessageLine("Panda bears like to eat bamboo.",
                                {Link(6, 11, "http://www.example.com/0"),
                                 Link(24, 30, "http://www.example.com/1"),
                                 Link(17, 19, "http://www.example.com/2")})}},
      TestCase{"{"
               "  \"line\" : [ {"
               "    \"template\": \"Panda {0}\","
               "    \"template_parameter\": [ {"
               "      \"display_text\": \"bears\","
               "      \"url\": \"http://www.example.com/line_0_param_0\""
               "    } ]"
               "  }, {"
               "    \"template\": \"like {1} eat {0}.\","
               "    \"template_parameter\": [ {"
               "      \"display_text\": \"bamboo\","
               "      \"url\": \"http://www.example.com/line_1_param_0\""
               "    }, {"
               "      \"display_text\": \"to\","
               "      \"url\": \"http://www.example.com/line_1_param_1\""
               "    } ]"
               "  }, {"
               "    \"template\": \"The {0}.\","
               "    \"template_parameter\": [ {"
               "      \"display_text\": \"end\","
               "      \"url\": \"http://www.example.com/line_2_param_0\""
               "    } ]"
               "  } ]"
               "}",
               {TestLegalMessageLine(
                    "Panda bears",
                    {Link(6, 11, "http://www.example.com/line_0_param_0")}),
                TestLegalMessageLine(
                    "like to eat bamboo.",
                    {Link(12, 18, "http://www.example.com/line_1_param_0"),
                     Link(5, 7, "http://www.example.com/line_1_param_1")}),
                TestLegalMessageLine(
                    "The end.",
                    {Link(4, 7, "http://www.example.com/line_2_param_0")})

               }},
      TestCase{
          "{"
          "  \"line\" : [ {"
          "    \"template\": \"Panda {0}\nlike {2} eat {1}.\nThe {3}.\","
          "    \"template_parameter\": [ {"
          "      \"display_text\": \"bears\","
          "      \"url\": \"http://www.example.com/0\""
          "    }, {"
          "      \"display_text\": \"bamboo\","
          "      \"url\": \"http://www.example.com/1\""
          "    }, {"
          "      \"display_text\": \"to\","
          "      \"url\": \"http://www.example.com/2\""
          "    }, {"
          "      \"display_text\": \"end\","
          "      \"url\": \"http://www.example.com/3\""
          "    } ]"
          "  } ]"
          "}",
          {TestLegalMessageLine("Panda bears\nlike to eat bamboo.\nThe end.",
                                {Link(6, 11, "http://www.example.com/0"),
                                 Link(24, 30, "http://www.example.com/1"),
                                 Link(17, 19, "http://www.example.com/2"),
                                 Link(36, 39, "http://www.example.com/3")})}},
      TestCase{
          "{"
          "  \"line\" : [ {"
          "    \"template\": \"a{0} b{1} c{2} d{3} e{4} f{5} g{6}\","
          "    \"template_parameter\": [ {"
          "      \"display_text\": \"A\","
          "      \"url\": \"http://www.example.com/0\""
          "    }, {"
          "      \"display_text\": \"B\","
          "      \"url\": \"http://www.example.com/1\""
          "    }, {"
          "      \"display_text\": \"C\","
          "      \"url\": \"http://www.example.com/2\""
          "    }, {"
          "      \"display_text\": \"D\","
          "      \"url\": \"http://www.example.com/3\""
          "    }, {"
          "      \"display_text\": \"E\","
          "      \"url\": \"http://www.example.com/4\""
          "    }, {"
          "      \"display_text\": \"F\","
          "      \"url\": \"http://www.example.com/5\""
          "    }, {"
          "      \"display_text\": \"G\","
          "      \"url\": \"http://www.example.com/6\""
          "    } ]"
          "  } ]"
          "}",
          {TestLegalMessageLine("aA bB cC dD eE fF gG",
                                {Link(1, 2, "http://www.example.com/0"),
                                 Link(4, 5, "http://www.example.com/1"),
                                 Link(7, 8, "http://www.example.com/2"),
                                 Link(10, 11, "http://www.example.com/3"),
                                 Link(13, 14, "http://www.example.com/4"),
                                 Link(16, 17, "http://www.example.com/5"),
                                 Link(19, 20, "http://www.example.com/6")})}},
      // When |escape_apostrophes| is true, all ASCII apostrophes should be
      // escaped for ICU's MessageFormat by doubling them up.  This allows the
      // template parameters to work correctly.
      // http://www.icu-project.org/apiref/icu4c/messagepattern_8h.html#af6e0757e0eb81c980b01ee5d68a9978b
      TestCase{
          "{"
          "  \"line\" : [ {"
          "    \"template\": \"The panda bear's bamboo was '{0}.\","
          "    \"template_parameter\": [ {"
          "      \"display_text\": \"delicious\","
          "      \"url\": \"http://www.example.com/0\""
          "    } ]"
          "  } ]"
          "}",
          {TestLegalMessageLine("The panda bear's bamboo was 'delicious.",
                                {Link(29, 38, "http://www.example.com/0")})},
          true},
  }};
  CHECK_EQ(cases->size(), TestCaseValues().size());
  return *cases;
}

class LegalMessageLineTest : public ::testing::TestWithParam<int> {
 public:
  LegalMessageLineTest() = default;
  ~LegalMessageLineTest() override = default;
};

// Verifies that legal message parsing is correct.
TEST_P(LegalMessageLineTest, Parsing) {
  const TestCase& test_case = TestCaseData()[GetParam()];
  std::optional<base::Value> value(
      base::JSONReader::Read(test_case.message_json));
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_dict());
  LegalMessageLines actual_lines;
  LegalMessageLine::Parse(value->GetDict(), &actual_lines,
                          test_case.escape_apostrophes);

  EXPECT_EQ(test_case.expected_lines, actual_lines);
}

INSTANTIATE_TEST_SUITE_P(TestCases,
                         LegalMessageLineTest,
                         testing::ValuesIn(TestCaseValues()));

}  // namespace
}  // namespace autofill
