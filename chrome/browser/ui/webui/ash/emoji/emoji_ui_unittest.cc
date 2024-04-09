// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "emoji_ui.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/fake_text_input_client.h"

class EmojiUITest : public testing::Test {
 protected:
  std::unique_ptr<ui::TextInputClient> none_input_client =
      std::make_unique<ui::FakeTextInputClient>(ui::TEXT_INPUT_TYPE_NONE);
};

TEST_F(EmojiUITest, ShouldShow) {
  ASSERT_EQ(ash::EmojiUI::ShouldShow(
                none_input_client.get(),
                ui::EmojiPickerFocusBehavior::kOnlyShowWhenFocused),
            true);
}

TEST_F(EmojiUITest, ShouldNotShowWithoutInputClient) {
  ASSERT_EQ(ash::EmojiUI::ShouldShow(
                nullptr, ui::EmojiPickerFocusBehavior::kOnlyShowWhenFocused),
            false);
}

TEST_F(EmojiUITest, ShouldShowWithoutInputClient) {
  ASSERT_EQ(ash::EmojiUI::ShouldShow(nullptr,
                                     ui::EmojiPickerFocusBehavior::kAlwaysShow),
            true);
}
