// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/csv_reader.h"

#include <utility>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

TEST(CSVReaderTest, Positive) {
  struct TestCase {
    const char* name;
    const std::string input;
    std::vector<const char*> expected_column_names;
    std::vector<std::vector<std::pair<const char*, const char*>>>
        expected_row_maps;
  };
  const TestCase kCases[] = {
      {
          "EmptyCSV",
          "",
          {""},
          {},
      },
      {
          "CSVConsistingOfSingleNewLine",
          "\n",
          {""},
          {},
      },
      {
          "SingleColumn",
          "foo\n"
          "alpha\n"
          "beta\n",
          {"foo"},
          {{{"foo", "alpha"}}, {{"foo", "beta"}}},
      },
      {
          "HeaderOnly",
          "foo,bar\n",
          {"foo", "bar"},
          {},
      },
      {
          "NoNewline",
          "foo,bar",
          {"foo", "bar"},
          {},
      },
      {
          "HeaderAndSimpleRecords",
          "foo,bar,baz\n"
          "alpha,beta,gamma\n"
          "delta,epsilon,zeta\n",
          {"foo", "bar", "baz"},
          {{{"bar", "beta"}, {"baz", "gamma"}, {"foo", "alpha"}},
           {{"bar", "epsilon"}, {"baz", "zeta"}, {"foo", "delta"}}},
      },
      {
          "EmptyStringColumnNamesAreSupported",
          "foo,,bar\n"
          "alpha,beta,gamma\n",
          {"foo", "", "bar"},
          {{{"", "beta"}, {"bar", "gamma"}, {"foo", "alpha"}}},
      },
      {
          "ExtraSpacesArePreserved",
          "left,right\n"
          " alpha  beta ,  \n",
          {"left", "right"},
          {{{"left", " alpha  beta "}, {"right", "  "}}},
      },
      {
          "CharactersOutsideASCIIPrintableArePreservedVerbatim",
          "left,right\n"
          "\x07\t\x0B\x1F,$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2\n",
          {"left", "right"},
          {{// Characters below 0x20: bell, horizontal + vertical tab, unit
            // separator.
            {"left", "\x07\t\x0B\x1F"},
            // Unicode code points having 1..4 byte UTF-8 representation: dollar
            // sign (U+0024), cent sign (U+00A2), snowman (U+2603), Han
            // character U+24B62.
            {"right", "$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2"}}},
      },
      {
          "EnclosingDoubleQuotesAreTrimmed",
          "\"left\",\"right\"\n"
          "\"alpha\",\"beta\"\n",
          {"left", "right"},
          {{{"left", "alpha"}, {"right", "beta"}}},
      },
      {
          "SeparatorsInsideDoubleQuotesAreTreatedVerbatim",
          "left,right\n"
          "\"A\rB\",\"B\nC\"\n"
          "\"C\r\nD\",\"D\n\"\n"
          "\",\",\",,\"\n",
          {"left", "right"},
          {{{"left", "A\rB"}, {"right", "B\nC"}},
           {{"left", "C\r\nD"}, {"right", "D\n"}},
           {{"left", ","}, {"right", ",,"}}},
      },
      {
          "EscapedDoubleQuotes",
          "left,right\n"
          R"("","""""")"
          "\n"
          R"("""","A""B""""C")"
          "\n",
          {"left", "right"},
          {{{"left", ""}, {"right", "\"\""}},
           {{"left", "\""}, {"right", "A\"B\"\"C"}}},
      },
      {
          "InconsistentFieldsCountIsTreatedGracefully",
          "left,right\n"
          "A\n"
          "B,C,D\n",
          {"left", "right"},
          {{{"left", "A"}}, {{"left", "B"}, {"right", "C"}}},
      },
      {
          "SupportMissingNewLineAtEOF",
          "left,right\n"
          "alpha,beta",
          {"left", "right"},
          {{{"left", "alpha"}, {"right", "beta"}}},
      },
      {
          "EmptyFields",
          "left,middle,right\n"
          "alpha,beta,\n"
          ",,gamma\n",
          {"left", "middle", "right"},
          {{{"left", "alpha"}, {"middle", "beta"}, {"right", ""}},
           {{"left", ""}, {"middle", ""}, {"right", "gamma"}}},
      },
      {
          "CRLFTreatedAsLF",
          "left,right\r\n"
          "\"\r\",\"\r\n\"\r\n",
          {"left", "right"},
          {{{"left", "\r"}, {"right", "\r\n"}}},
      },
      {
          "CRAloneIgnored",
          "left,right\r"
          "A,B\r\n"
          "1,2,3",
          {"left", "right\rA", "B"},
          {{{"B", "3"}, {"left", "1"}, {"right\rA", "2"}}},
      },
      {
          "LastValueForRepeatedColumnNamesIsPreserved",
          "foo,bar,bar\n"
          "alpha,beta,gamma\n",
          {"foo", "bar", "bar"},
          {{{"bar", "gamma"}, {"foo", "alpha"}}},
      },
      {
          "EmptyLastFieldAndNoNewline",
          "alpha,",
          {"alpha", ""},
          {},
      },
      {
          "EmptyLinesIgnored",
          "foo,bar\n"
          "\n"
          "a,b\n"
          "\r\n"
          "c,d\r\r\r\r\r\r\r\r\n"
          "e,f",
          {"foo", "bar"},
          {
              {{"bar", "b"}, {"foo", "a"}},
              {{"bar", "d\r\r\r\r\r\r\r"}, {"foo", "c"}},
              {{"bar", "f"}, {"foo", "e"}},
          },
      },
  };

  for (const TestCase& test_case : kCases) {
    SCOPED_TRACE(test_case.name);
    CSVTable table;
    ASSERT_TRUE(table.ReadCSV(test_case.input));

    EXPECT_THAT(table.column_names(),
                testing::ElementsAreArray(test_case.expected_column_names));
    ASSERT_EQ(test_case.expected_row_maps.size(), table.records().size());
    for (size_t i = 0; i < test_case.expected_row_maps.size(); ++i) {
      SCOPED_TRACE(i);
      EXPECT_THAT(table.records()[i],
                  testing::ElementsAreArray(test_case.expected_row_maps[i]));
    }
  }
}

TEST(CSVReaderTest, Negative) {
  struct TestCase {
    const char* name;
    const std::string input;
  };
  const TestCase kCases[] = {
      {
          "FailureWhenEOFInsideQuotes",
          "left,right\n"
          "\"alpha\",\"unmatched\n",
      },
      {
          "FailureWhenSemiQuotedContentInHeader",
          "\"a\"b\"c\",right\n"
          "alpha,beta\n",
      },
      {
          "FailureWhenSemiQuotedContentOnSubsequentLine",
          "alpha,beta\n"
          "left,\"a\"b\"c\"\n",
      },
      {
          "FailureWhenJustOneQuote",
          "\"",
      },
      {
          "FailureWhenJustOneQuoteAndComma",
          "\",",
      },
  };

  for (const TestCase& test_case : kCases) {
    SCOPED_TRACE(test_case.name);
    CSVTable table;
    EXPECT_FALSE(table.ReadCSV(test_case.input));
  }
}

}  // namespace password_manager
