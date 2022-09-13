// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree.h"

#include <utility>

#include "pdf/accessibility_structs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace pdf {

const chrome_pdf::AccessibilityTextRunInfo kFirstTextRun = {
    15, gfx::RectF(26.0f, 189.0f, 84.0f, 13.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityTextRunInfo kSecondTextRun = {
    15, gfx::RectF(28.0f, 117.0f, 152.0f, 19.0f),
    chrome_pdf::AccessibilityTextDirection::kNone,
    chrome_pdf::AccessibilityTextStyleInfo()};
const chrome_pdf::AccessibilityCharInfo kDummyCharsData[] = {
    {'H', 12}, {'e', 6},  {'l', 5},  {'l', 4},  {'o', 8},  {',', 4},
    {' ', 4},  {'w', 12}, {'o', 6},  {'r', 6},  {'l', 4},  {'d', 9},
    {'!', 4},  {'\r', 0}, {'\n', 0}, {'G', 16}, {'o', 12}, {'o', 12},
    {'d', 12}, {'b', 10}, {'y', 12}, {'e', 12}, {',', 4},  {' ', 6},
    {'w', 16}, {'o', 12}, {'r', 8},  {'l', 4},  {'d', 12}, {'!', 2},
};

TEST(PdfAccessibilityTreeUnitTest, TextRunsAndCharsMismatch) {
  // `chars` and `text_runs` span over the same page text. They should denote
  // the same page text size, but `text_runs_` is incorrect and only denotes 1
  // of 2 text runs.
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, TextRunsAndCharsMatch) {
  // `chars` and `text_runs` span over the same page text. They should denote
  // the same page text size.
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  EXPECT_TRUE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                          page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, UnsortedLinkVector) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    // Add first link in the vector.
    chrome_pdf::AccessibilityLinkInfo link;
    link.text_range.index = 2;
    link.text_range.count = 0;
    link.index_in_page = 0;
    page_objects.links.push_back(std::move(link));
  }

  {
    // Add second link in the vector.
    chrome_pdf::AccessibilityLinkInfo link;
    link.text_range.index = 0;
    link.text_range.count = 1;
    link.index_in_page = 1;
    page_objects.links.push_back(std::move(link));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundLink) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityLinkInfo link;
    link.text_range.index = 3;
    link.text_range.count = 0;
    link.index_in_page = 0;
    page_objects.links.push_back(std::move(link));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, UnsortedImageVector) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    // Add first image to the vector.
    chrome_pdf::AccessibilityImageInfo image;
    image.text_run_index = 1;
    page_objects.images.push_back(std::move(image));
  }

  {
    // Add second image to the vector.
    chrome_pdf::AccessibilityImageInfo image;
    image.text_run_index = 0;
    page_objects.images.push_back(std::move(image));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundImage) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityImageInfo image;
    image.text_run_index = 3;
    page_objects.images.push_back(std::move(image));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, UnsortedHighlightVector) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    // Add first highlight in the vector.
    chrome_pdf::AccessibilityHighlightInfo highlight;
    highlight.text_range.index = 2;
    highlight.text_range.count = 0;
    highlight.index_in_page = 0;
    page_objects.highlights.push_back(std::move(highlight));
  }

  {
    // Add second highlight in the vector.
    chrome_pdf::AccessibilityHighlightInfo highlight;
    highlight.text_range.index = 0;
    highlight.text_range.count = 1;
    highlight.index_in_page = 1;
    page_objects.highlights.push_back(std::move(highlight));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundHighlight) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityHighlightInfo highlight;
    highlight.text_range.index = 3;
    highlight.text_range.count = 0;
    highlight.index_in_page = 0;
    page_objects.highlights.push_back(std::move(highlight));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, UnsortedTextFieldVector) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    // Add first text field in the vector.
    chrome_pdf::AccessibilityTextFieldInfo text_field;
    text_field.text_run_index = 2;
    text_field.index_in_page = 0;
    page_objects.form_fields.text_fields.push_back(std::move(text_field));
  }

  {
    // Add second text field in the vector.
    chrome_pdf::AccessibilityTextFieldInfo text_field;
    text_field.text_run_index = 0;
    text_field.index_in_page = 1;
    page_objects.form_fields.text_fields.push_back(std::move(text_field));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundTextField) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityTextFieldInfo text_field;
    text_field.text_run_index = 3;
    text_field.index_in_page = 0;
    page_objects.form_fields.text_fields.push_back(std::move(text_field));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, UnsortedChoiceFieldVector) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    // Add first choice field in the vector.
    chrome_pdf::AccessibilityChoiceFieldInfo choice_field;
    choice_field.text_run_index = 2;
    choice_field.index_in_page = 0;
    page_objects.form_fields.choice_fields.push_back(std::move(choice_field));
  }

  {
    // Add second choice field in the vector.
    chrome_pdf::AccessibilityChoiceFieldInfo choice_field;
    choice_field.text_run_index = 0;
    choice_field.index_in_page = 1;
    page_objects.form_fields.choice_fields.push_back(std::move(choice_field));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundChoiceField) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityChoiceFieldInfo choice_field;
    choice_field.text_run_index = 3;
    choice_field.index_in_page = 0;
    page_objects.form_fields.choice_fields.push_back(std::move(choice_field));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, UnsortedButtonVector) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    // Add first button in the vector.
    chrome_pdf::AccessibilityButtonInfo button;
    button.text_run_index = 2;
    button.index_in_page = 0;
    page_objects.form_fields.buttons.push_back(std::move(button));
  }

  {
    // Add second button in the vector.
    chrome_pdf::AccessibilityButtonInfo button;
    button.text_run_index = 0;
    button.index_in_page = 1;
    page_objects.form_fields.buttons.push_back(std::move(button));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundButton) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityButtonInfo button;
    button.text_run_index = 3;
    button.index_in_page = 0;
    page_objects.form_fields.buttons.push_back(std::move(button));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundRadioButton) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityButtonInfo button;
    button.type = chrome_pdf::ButtonType::kRadioButton;
    button.text_run_index = 0;
    button.control_index = 1;
    button.control_count = 2;
    button.index_in_page = 0;
    page_objects.form_fields.buttons.push_back(std::move(button));
  }

  EXPECT_TRUE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                          page_objects));

  {
    chrome_pdf::AccessibilityButtonInfo button;
    button.type = chrome_pdf::ButtonType::kRadioButton;
    button.text_run_index = 0;
    button.control_index = 3;
    button.control_count = 2;
    button.index_in_page = 1;
    page_objects.form_fields.buttons.push_back(std::move(button));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundCheckBox) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityButtonInfo button;
    button.type = chrome_pdf::ButtonType::kCheckBox;
    button.text_run_index = 0;
    button.control_index = 1;
    button.control_count = 2;
    button.index_in_page = 0;
    page_objects.form_fields.buttons.push_back(std::move(button));
  }

  EXPECT_TRUE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                          page_objects));

  {
    chrome_pdf::AccessibilityButtonInfo button;
    button.type = chrome_pdf::ButtonType::kCheckBox;
    button.text_run_index = 0;
    button.control_index = 3;
    button.control_count = 2;
    button.index_in_page = 1;
    page_objects.form_fields.buttons.push_back(std::move(button));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, InvalidButtonType) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityButtonInfo button;
    button.type = static_cast<chrome_pdf::ButtonType>(666);
    button.text_run_index = 0;
    button.control_index = 1;
    button.control_count = 2;
    button.index_in_page = 0;
    page_objects.form_fields.buttons.push_back(std::move(button));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundIndexInPageLink) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    // Add first link in the vector.
    chrome_pdf::AccessibilityLinkInfo link;
    link.text_range.index = 1;
    link.text_range.count = 0;
    link.index_in_page = 1;
    page_objects.links.push_back(std::move(link));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundIndexInPageHighlight) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityHighlightInfo highlight;
    highlight.text_range.index = 1;
    highlight.text_range.count = 0;
    highlight.index_in_page = 1;
    page_objects.highlights.push_back(std::move(highlight));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundIndexInPageTextFeild) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityTextFieldInfo text_feild;
    text_feild.text_run_index = 1;
    text_feild.index_in_page = 1;
    page_objects.form_fields.text_fields.push_back(std::move(text_feild));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, OutOfBoundIndexInChoiceFeild) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityChoiceFieldInfo choice_field;
    choice_field.type = chrome_pdf::ChoiceFieldType::kListBox;
    choice_field.text_run_index = 2;
    choice_field.index_in_page = 1;
    page_objects.form_fields.choice_fields.push_back(std::move(choice_field));
  }
  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

TEST(PdfAccessibilityTreeUnitTest, InvalidChoiceFieldType) {
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  text_runs.emplace_back(kFirstTextRun);
  text_runs.emplace_back(kSecondTextRun);

  std::vector<chrome_pdf::AccessibilityCharInfo> chars(
      std::begin(kDummyCharsData), std::end(kDummyCharsData));

  chrome_pdf::AccessibilityPageObjects page_objects;

  {
    chrome_pdf::AccessibilityChoiceFieldInfo choice_field;
    choice_field.type = static_cast<chrome_pdf::ChoiceFieldType>(666);
    choice_field.text_run_index = 0;
    choice_field.index_in_page = 0;
    page_objects.form_fields.choice_fields.push_back(std::move(choice_field));
  }

  EXPECT_FALSE(PdfAccessibilityTree::IsDataFromPluginValid(text_runs, chars,
                                                           page_objects));
}

}  // namespace pdf
