// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "components/compose/core/browser/compose_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::TestWithParam;

namespace compose {

struct TrimmingTestCase {
  std::string test_name;
  std::string page_text;
  int max_chars = 0;
  int active_element_offset = 0;
  int header_budget = 0;
  std::string expected;
};

using ComposePromptTrimmingUnitTest = TestWithParam<TrimmingTestCase>;

TEST_P(ComposePromptTrimmingUnitTest, TestPromptTrimming) {
  const TrimmingTestCase& test_case = GetParam();

  ASSERT_EQ(GetTrimmedPageText(test_case.page_text, test_case.max_chars,
                               test_case.active_element_offset,
                               test_case.header_budget),
            test_case.expected);
}

INSTANTIATE_TEST_SUITE_P(
    ComposePromptTrimmingUnitTestSuiteInstantiation,
    ComposePromptTrimmingUnitTest,
    testing::ValuesIn<TrimmingTestCase>({
        {
            "string_with_nothing_to_scrub",
            "a short string with nothing to scrub",
            60,
            40,
            20,
            "a short string with nothing to scrub",
        },
        {
            "string_longer_than_maxLength",
            "a loooooooooooooooooooooooong string",
            15,
            50,
            5,
            "a loo...\nstring",
        },
        {
            "string_longer_than_maxLength_with_a_central_offset",
            "a longer string with an interior offset in the string",
            20,
            20,
            3,
            "a l...\nr string with",
        },
        {
            "string_where_the_header_and_offset_overlap",
            "a longer string with an interior offset in the string",
            30,
            25,
            20,
            "a longer string with an interi",
        },
        {
            "string_where_the_header_is_longer_than_the_content",
            "a longer string with an interior offset in the string",
            30,
            25,
            35,
            "a longer string with an interi",
        },

        {"string_of_unicorn_emojis",
         "🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄🦄", 15, 50, 5, "🦄...\n🦄"},
    }),
    [](const testing::TestParamInfo<ComposePromptTrimmingUnitTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace compose
