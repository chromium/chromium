// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_field_parser.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

TEST(CSVFieldParser, Positive) {
  struct TestCase {
    const char* name;
    const std::string input;
    std::vector<std::string> expected_fields;
  };
  const auto kCases = std::to_array<TestCase>(
      {{"empty CSV", "", {""}},
       {"single field", "foo", {"foo"}},
       {"two fields", "foo,bar", {"foo", "bar"}},
       {"empty string", "foo,,bar", {"foo", "", "bar"}},
       {"extra spaces", " alpha  beta ,  ", {" alpha  beta ", "  "}},
       {"no ASCII printable",
        "\x07\t\x0B\x1F,$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2",
        {// Characters below 0x20: bell, horizontal + vertical tab, unit
         // separator.
         "\x07\t\x0B\x1F",
         // Unicode code points having 1..4 byte UTF-8 representation: dollar
         // sign (U+0024), cent sign (U+00A2), snowman (U+2603), Han
         // character U+24B62.
         "$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2"}},
       {"escaping", "\"A\",\"B\"", {"A", "B"}},
       {"escaped separators",
        "\"A\rB\",\"B\nC\",\"C\r\nD\",\"D\n\",\",\",\",,\",\"\"\"\"",
        {"A\rB", "B\nC", "C\r\nD", "D\n", ",", ",,", "\"\""}},
       {"empty fields", ",,gamma", {"", "", "gamma"}},
       {"just enough fields", std::string(CSVFieldParser::kMaxFields - 1, ','),
        std::vector<std::string>(CSVFieldParser::kMaxFields)}});

  for (const TestCase& test_case : kCases) {
    SCOPED_TRACE(test_case.name);
    CSVFieldParser field_parser(test_case.input);

    for (const std::string& field : test_case.expected_fields) {
      ASSERT_TRUE(field_parser.HasMoreFields());
      std::string_view parsed;
      EXPECT_TRUE(field_parser.NextField(&parsed));
      EXPECT_EQ(field, parsed);
    }
  }
}

TEST(CSVFieldParser, Negative) {
  struct TestCase {
    const char* name;
    const std::string input;
    size_t index_of_first_failure;
  };
  const auto kCases = std::to_array<TestCase>(
      {{"unmatched quote, no text", "\"", 0},
       {"unmatched quote, comma", "j,\",", 1},
       {"unmatched quotes", "\"alpha\",\"unmatched", 1},
       {"wrong quotes", "\"a\"b\"c\",right", 0},
       {"too many fields", std::string(CSVFieldParser::kMaxFields, ','),
        CSVFieldParser::kMaxFields}});

  for (const TestCase& test_case : kCases) {
    SCOPED_TRACE(test_case.name);
    CSVFieldParser field_parser(test_case.input);

    std::string_view parsed;
    for (size_t i = 0; i < test_case.index_of_first_failure; ++i) {
      ASSERT_TRUE(field_parser.HasMoreFields());
      EXPECT_TRUE(field_parser.NextField(&parsed));
    }
    ASSERT_TRUE(field_parser.HasMoreFields());
    EXPECT_FALSE(field_parser.NextField(&parsed));
  }
}

}  // namespace password_manager
