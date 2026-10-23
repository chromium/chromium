// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/text_field_filter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace dictation {
namespace {

using content::EditableLevel;

TEST(DictationTextFieldFilterTest, NotEditable) {
  EXPECT_EQ(GetTextFieldSize(EditableLevel::kNotEditable, std::nullopt),
            TextFieldSize::kNotEditable);
}

TEST(DictationTextFieldFilterTest, RichlyEditableIsMediumOrLarge) {
  // The text input type isn't needed to decide.
  EXPECT_EQ(GetTextFieldSize(EditableLevel::kRichlyEditable, std::nullopt),
            TextFieldSize::kMediumOrLarge);
}

TEST(DictationTextFieldFilterTest, TextAreaAndContentEditableAreMediumOrLarge) {
  EXPECT_EQ(GetTextFieldSize(EditableLevel::kPlaintextEditable,
                             ui::TEXT_INPUT_TYPE_TEXT_AREA),
            TextFieldSize::kMediumOrLarge);
  EXPECT_EQ(GetTextFieldSize(EditableLevel::kPlaintextEditable,
                             ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE),
            TextFieldSize::kMediumOrLarge);
}

TEST(DictationTextFieldFilterTest, OneLineInputsAreSmall) {
  for (ui::TextInputType type :
       {ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_TYPE_SEARCH,
        ui::TEXT_INPUT_TYPE_EMAIL, ui::TEXT_INPUT_TYPE_URL,
        ui::TEXT_INPUT_TYPE_TELEPHONE, ui::TEXT_INPUT_TYPE_NUMBER,
        ui::TEXT_INPUT_TYPE_PASSWORD}) {
    EXPECT_EQ(GetTextFieldSize(EditableLevel::kPlaintextEditable, type),
              TextFieldSize::kSmall)
        << type;
  }
}

TEST(DictationTextFieldFilterTest, PlaintextWithoutTypeIsUnknown) {
  EXPECT_EQ(GetTextFieldSize(EditableLevel::kPlaintextEditable, std::nullopt),
            TextFieldSize::kUnknown);
}

}  // namespace
}  // namespace dictation
