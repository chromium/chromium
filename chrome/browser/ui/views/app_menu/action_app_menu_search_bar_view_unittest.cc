// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_search_bar_view.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/image_view.h"

namespace {

class ActionAppMenuSearchBarViewTest : public ChromeViewsTestBase {
 public:
  ActionAppMenuSearchBarViewTest() = default;
  ~ActionAppMenuSearchBarViewTest() override = default;
};

TEST_F(ActionAppMenuSearchBarViewTest, InitialProperties) {
  auto search_bar = std::make_unique<ActionAppMenuSearchBarView>();

  views::ImageView* icon = search_bar->search_icon_for_testing();
  ASSERT_NE(icon, nullptr);

  EXPECT_TRUE(search_bar->GetText().empty());
  EXPECT_EQ(search_bar->GetPlaceholderText(),
            l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_PLACEHOLDER));
  EXPECT_EQ(search_bar->GetPlaceholderText(),
            u"Search menu, or type an action");
  EXPECT_EQ(search_bar->placeholder_text_color_id(),
            ui::kColorTextfieldForegroundPlaceholder);
  EXPECT_TRUE(icon->GetVisible());

  // Verify border with insets and background is transparent.
  ASSERT_NE(search_bar->GetBorder(), nullptr);
  const auto* provider = ChromeLayoutProvider::Get();
  int icon_size =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_ICON_SIZE);
  int icon_padding = 12;
  int icon_text_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL);
  int left_inset = icon_padding + icon_size + icon_text_spacing;
  EXPECT_EQ(search_bar->GetBorder()->GetInsets(),
            gfx::Insets::TLBR(6, left_inset, 6, 12));
  EXPECT_EQ(search_bar->GetBackgroundColor(), SK_ColorTRANSPARENT);

  // Verify inkdrop is enabled.
  EXPECT_EQ(views::InkDrop::Get(search_bar.get())->GetMode(),
            views::InkDropHost::InkDropMode::ON);
}

TEST_F(ActionAppMenuSearchBarViewTest, MouseAndKeyboardInteractions) {
  auto search_bar = std::make_unique<ActionAppMenuSearchBarView>();
  views::ImageView* icon = search_bar->search_icon_for_testing();

  // Mouse press activates focus and enables cursor.
  EXPECT_TRUE(search_bar->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON)));
  EXPECT_TRUE(search_bar->is_active_for_testing());
  EXPECT_TRUE(search_bar->GetCursorEnabled());

  // Type characters into search bar.
  ui::KeyEvent key_a(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  key_a.set_character('a');
  search_bar->HandleKeyEvent(&key_a);
  EXPECT_EQ(search_bar->GetText(), u"a");
  EXPECT_TRUE(icon->GetVisible());

  ui::KeyEvent key_b(ui::EventType::kKeyPressed, ui::VKEY_B, ui::EF_NONE);
  key_b.set_character('b');
  search_bar->HandleKeyEvent(&key_b);
  EXPECT_EQ(search_bar->GetText(), u"ab");

  // Hitting Enter should do nothing and preserve text.
  ui::KeyEvent enter_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                           ui::EF_NONE);
  search_bar->HandleKeyEvent(&enter_event);
  EXPECT_EQ(search_bar->GetText(), u"ab");

  // Hitting Backspace deletes previous character.
  ui::KeyEvent backspace_event(ui::EventType::kKeyPressed, ui::VKEY_BACK,
                               ui::EF_NONE);
  search_bar->HandleKeyEvent(&backspace_event);
  EXPECT_EQ(search_bar->GetText(), u"a");

  search_bar->HandleKeyEvent(&backspace_event);
  EXPECT_EQ(search_bar->GetText(), u"");
}

TEST_F(ActionAppMenuSearchBarViewTest, InactiveIgnoresTyping) {
  auto search_bar = std::make_unique<ActionAppMenuSearchBarView>();
  EXPECT_FALSE(search_bar->is_active_for_testing());

  ui::KeyEvent key_a(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  key_a.set_character('a');
  search_bar->HandleKeyEvent(&key_a);
  EXPECT_FALSE(key_a.handled());
  EXPECT_TRUE(search_bar->GetText().empty());
}

TEST_F(ActionAppMenuSearchBarViewTest, ActiveOnAddedToWidget) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* search_bar =
      widget->SetContentsView(std::make_unique<ActionAppMenuSearchBarView>());
  EXPECT_TRUE(search_bar->is_active_for_testing());
  EXPECT_TRUE(search_bar->GetCursorEnabled());
}

TEST_F(ActionAppMenuSearchBarViewTest, DownArrowDeactivatesFocus) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* search_bar =
      widget->SetContentsView(std::make_unique<ActionAppMenuSearchBarView>());
  EXPECT_TRUE(search_bar->is_active_for_testing());

  ui::KeyEvent down_key(ui::EventType::kKeyPressed, ui::VKEY_DOWN, ui::EF_NONE);
  search_bar->HandleKeyEvent(&down_key);
  EXPECT_FALSE(search_bar->is_active_for_testing());
  EXPECT_FALSE(search_bar->GetCursorEnabled());
  // Down key should be unhandled so MenuController can process it.
  EXPECT_FALSE(down_key.handled());
}

}  // namespace
