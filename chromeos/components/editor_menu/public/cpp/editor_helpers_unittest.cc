// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/editor_menu/public/cpp/editor_helpers.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::editor_helpers {
namespace {

using ::testing::TestWithParam;

struct StripWhitespaceTestCase {
  std::string test_name;

  std::u16string text;
  gfx::Range selection_range;

  size_t stripped_length;
};

using StripWhitespaceTest = TestWithParam<StripWhitespaceTestCase>;

INSTANTIATE_TEST_SUITE_P(
    StripWhitespaceTests,
    StripWhitespaceTest,
    testing::ValuesIn<StripWhitespaceTestCase>({
        {.test_name = "EmptyString",
         .text = u"",
         .selection_range = {0, 0},
         .stripped_length = 0},
        {.test_name = "SingleSpace",
         .text = u" ",
         .selection_range = {0, 1},
         .stripped_length = 0},
        {.test_name = "SingleLetter",
         .text = u"a",
         .selection_range = {0, 1},
         .stripped_length = 1},
        {.test_name = "PrependedWhitespace",
         .text = u"    abc abc abc ",
         .selection_range = {0, 4},
         .stripped_length = 0},
        {.test_name = "apendedWhitespace",
         .text = u"    abc abc abc    ",
         .selection_range = {15, 19},
         .stripped_length = 0},
        {.test_name = "WhitespaceAndSingleCharAtStart",
         .text = u"    abc abc abc ",
         .selection_range = {0, 5},
         .stripped_length = 1},
        {.test_name = "WhitespaceAndSingleCharAtEnd",
         .text = u"    abc abc abc    ",
         .selection_range = {14, 19},
         .stripped_length = 1},
        {.test_name = "WhitespaceRemovedFromStartButNotCenter",
         .text = u"    abc abc abc ",
         .selection_range = {1, 13},
         .stripped_length = 9},
        {.test_name = "WhitespaceRemovedFromEndButNotCenter",
         .text = u"    abc abc abc   ",
         .selection_range = {9, 16},
         .stripped_length = 6},
        {.test_name = "SingleWhitespaceAtEnd",
         .text = u"    abc abc abc ",
         .selection_range = {15, 16},
         .stripped_length = 0},
        {.test_name = "SingleCharAndWhitespaceAtEnd",
         .text = u"    abc abc abc   ",
         .selection_range = {14, 16},
         .stripped_length = 1},
        {.test_name = "SingleWhitespaceAtEndAndSingleChar",
         .text = u"    abc abc abc   ",
         .selection_range = {14, 18},
         .stripped_length = 1},
        {.test_name = "WhitespaceOnBothEnds",
         .text = u"    abc abc abc   ",
         .selection_range = {0, 18},
         .stripped_length = 11},
    }),
    [](const testing::TestParamInfo<StripWhitespaceTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(StripWhitespaceTest, TestEditorStripWhitespace) {
  const StripWhitespaceTestCase& test_case = GetParam();

  EXPECT_EQ(
      NonWhitespaceAndSymbolsLength(test_case.text, test_case.selection_range),
      test_case.stripped_length);
}

}  // namespace
}  // namespace ash::input_method
