// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"

#include <memory>
#include <utility>

#include "base/i18n/language_tag.h"
#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

namespace {

class AppMenuSearchBarViewTest : public ChromeViewsTestBase {
 public:
  AppMenuSearchBarViewTest() = default;
  ~AppMenuSearchBarViewTest() override = default;

 protected:
  // Finds the search bar's child views by their element identifiers.
  views::Textfield* GetSearchField(AppMenuSearchBarView* search_bar) {
    return views::AsViewClass<views::Textfield>(search_bar->GetViewByElementId(
        AppMenuSearchBarView::kSearchFieldElementId));
  }

  views::View* GetSearchIcon(AppMenuSearchBarView* search_bar) {
    return search_bar->GetViewByElementId(
        AppMenuSearchBarView::kSearchIconElementId);
  }

  views::Button* GetBackButton(AppMenuSearchBarView* search_bar) {
    return views::Button::AsButton(search_bar->GetViewByElementId(
        AppMenuSearchBarView::kBackButtonElementId));
  }

  views::Button* GetClearButton(AppMenuSearchBarView* search_bar) {
    return views::Button::AsButton(search_bar->GetViewByElementId(
        AppMenuSearchBarView::kClearButtonElementId));
  }
};

TEST_F(AppMenuSearchBarViewTest, InitialProperties) {
  auto search_bar = std::make_unique<AppMenuSearchBarView>();
  views::Textfield* search_field = GetSearchField(search_bar.get());

  views::View* icon = GetSearchIcon(search_bar.get());
  ASSERT_NE(icon, nullptr);

  EXPECT_TRUE(search_bar->GetText().empty());
  EXPECT_EQ(search_field->GetPlaceholderText(),
            l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_PLACEHOLDER));
  EXPECT_EQ(search_field->GetPlaceholderText(),
            u"Search menu, or type an action");
  EXPECT_EQ(search_field->placeholder_text_color_id(),
            ui::kColorTextfieldForegroundPlaceholder);
  EXPECT_TRUE(icon->GetVisible());

  // Verify border with insets and background is transparent.
  ASSERT_NE(search_field->GetBorder(), nullptr);
  const auto* provider = ChromeLayoutProvider::Get();
  int icon_size =
      provider->GetDistanceMetric(DISTANCE_ACTION_APP_MENU_DEFAULT_ICON_SIZE);
  int icon_padding = 12;
  int icon_text_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL);
  int horizontal_inset = icon_padding + icon_size + icon_text_spacing;
  EXPECT_EQ(search_field->GetBorder()->GetInsets(),
            gfx::Insets::VH(6, horizontal_inset));
  EXPECT_EQ(search_field->GetPreferredSize().height(), 28);
  EXPECT_EQ(search_field->GetBackgroundColor(), SK_ColorTRANSPARENT);

  // Verify inkdrop is enabled.
  EXPECT_EQ(views::InkDrop::Get(search_field)->GetMode(),
            views::InkDropHost::InkDropMode::ON);
}

TEST_F(AppMenuSearchBarViewTest, MouseAndKeyboardInteractions) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* search_bar =
      widget->SetContentsView(std::make_unique<AppMenuSearchBarView>());
  widget->Show();

  views::Textfield* search_field = GetSearchField(search_bar);
  views::View* icon = GetSearchIcon(search_bar);

  // Starts unfocused when added to a widget so menu mnemonics remain active.
  EXPECT_FALSE(search_field->HasFocus());

  // Mouse press focuses the search field.
  EXPECT_TRUE(search_field->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON)));
  EXPECT_TRUE(search_field->HasFocus());

  // Insert text and verify Backspace deletes the previous character.
  search_field->InsertOrReplaceText(u"ab");
  EXPECT_EQ(search_bar->GetText(), u"ab");
  EXPECT_TRUE(icon->GetVisible());

  ui::KeyEvent backspace_event(ui::EventType::kKeyPressed, ui::VKEY_BACK,
                               ui::EF_NONE);
  EXPECT_TRUE(
      static_cast<views::View*>(search_field)->OnKeyPressed(backspace_event));
  EXPECT_EQ(search_bar->GetText(), u"a");

  // Pressing Escape when text is non-empty clears the text and consumes the key
  // so the hosting menu stays open.
  ui::KeyEvent escape_event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                            ui::EF_NONE);
  EXPECT_TRUE(
      static_cast<views::View*>(search_field)->OnKeyPressed(escape_event));
  EXPECT_TRUE(search_bar->GetText().empty());

  // Pressing Escape when text is already empty is not consumed so the hosting
  // menu can close.
  EXPECT_FALSE(
      static_cast<views::View*>(search_field)->OnKeyPressed(escape_event));
}

TEST_F(AppMenuSearchBarViewTest, LeadingIconToggles) {
  auto search_bar = std::make_unique<AppMenuSearchBarView>();
  views::View* icon = GetSearchIcon(search_bar.get());
  views::Button* back_button = GetBackButton(search_bar.get());
  ASSERT_NE(icon, nullptr);
  ASSERT_NE(back_button, nullptr);

  EXPECT_TRUE(icon->GetVisible());
  EXPECT_FALSE(back_button->GetVisible());
  EXPECT_EQ(back_button->GetFocusBehavior(), views::View::FocusBehavior::NEVER);

  search_bar->SetLeadingIcon(AppMenuSearchBarView::LeadingIcon::kBack);
  EXPECT_FALSE(icon->GetVisible());
  EXPECT_TRUE(back_button->GetVisible());

  search_bar->SetLeadingIcon(AppMenuSearchBarView::LeadingIcon::kSearch);
  EXPECT_TRUE(icon->GetVisible());
  EXPECT_FALSE(back_button->GetVisible());
}

TEST_F(AppMenuSearchBarViewTest, BackPressClearsText) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* search_bar =
      widget->SetContentsView(std::make_unique<AppMenuSearchBarView>());
  widget->Show();
  search_bar->SetText(u"downloads");
  search_bar->SetLeadingIcon(AppMenuSearchBarView::LeadingIcon::kBack);

  EXPECT_FALSE(search_bar->GetText().empty());

  views::test::ButtonTestApi(GetBackButton(search_bar))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_TRUE(search_bar->GetText().empty());
  EXPECT_TRUE(GetSearchField(search_bar)->HasFocus());
}

TEST_F(AppMenuSearchBarViewTest, BackPressNotifiesTextChanged) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* search_bar =
      widget->SetContentsView(std::make_unique<AppMenuSearchBarView>());
  search_bar->SetText(u"downloads");
  search_bar->SetLeadingIcon(AppMenuSearchBarView::LeadingIcon::kBack);

  int notifications = 0;
  base::CallbackListSubscription subscription =
      search_bar->AddTextChangedCallback(
          base::BindLambdaForTesting([&]() { ++notifications; }));

  views::test::ButtonTestApi(GetBackButton(search_bar))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_GT(notifications, 0);
  EXPECT_TRUE(search_bar->GetText().empty());
}

TEST_F(AppMenuSearchBarViewTest, BackPressNotifiesBackPressed) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* search_bar =
      widget->SetContentsView(std::make_unique<AppMenuSearchBarView>());
  search_bar->SetText(u"downloads");
  search_bar->SetLeadingIcon(AppMenuSearchBarView::LeadingIcon::kBack);

  int notifications = 0;
  base::CallbackListSubscription subscription =
      search_bar->AddBackPressedCallback(base::BindLambdaForTesting([&]() {
        ++notifications;
        // Runs before the query is cleared.
        EXPECT_EQ(search_bar->GetText(), u"downloads");
      }));

  views::test::ButtonTestApi(GetBackButton(search_bar))
      .NotifyDefaultMouseClick();

  EXPECT_EQ(notifications, 1);
  EXPECT_TRUE(search_bar->GetText().empty());
}

TEST_F(AppMenuSearchBarViewTest, ClearButtonClearsText) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* search_bar =
      widget->SetContentsView(std::make_unique<AppMenuSearchBarView>());
  widget->Show();
  views::Button* clear_button = GetClearButton(search_bar);
  ASSERT_NE(clear_button, nullptr);
  EXPECT_FALSE(clear_button->GetVisible());
  EXPECT_EQ(clear_button->GetFocusBehavior(),
            views::View::FocusBehavior::NEVER);
  EXPECT_EQ(clear_button->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_CLEAR_SEARCH));
  EXPECT_EQ(clear_button->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(IDS_CLEAR_SEARCH));

  search_bar->SetText(u"downloads");
  EXPECT_TRUE(clear_button->GetVisible());

  int text_changed_count = 0;
  base::CallbackListSubscription text_sub = search_bar->AddTextChangedCallback(
      base::BindLambdaForTesting([&]() { ++text_changed_count; }));

  int back_pressed_count = 0;
  base::CallbackListSubscription back_sub = search_bar->AddBackPressedCallback(
      base::BindLambdaForTesting([&]() { ++back_pressed_count; }));

  views::test::ButtonTestApi(clear_button).NotifyDefaultMouseClick();
  EXPECT_TRUE(search_bar->GetText().empty());
  EXPECT_FALSE(clear_button->GetVisible());
  EXPECT_TRUE(GetSearchField(search_bar)->HasFocus());
  EXPECT_GT(text_changed_count, 0);
  EXPECT_EQ(back_pressed_count, 0);
}

TEST_F(AppMenuSearchBarViewTest, RtlLayoutMirrorsIcons) {
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("ar"));
  ASSERT_TRUE(base::i18n::IsRTL());

  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* search_bar =
      widget->SetContentsView(std::make_unique<AppMenuSearchBarView>());
  widget->SetBounds(gfx::Rect(0, 0, 200, 40));
  widget->LayoutRootViewIfNecessary();

  views::Textfield* search_field = GetSearchField(search_bar);
  views::View* search_icon = GetSearchIcon(search_bar);
  views::Button* back_button = GetBackButton(search_bar);
  views::Button* clear_button = GetClearButton(search_bar);

  EXPECT_GT(search_icon->GetMirroredBounds().x(), search_field->width() / 2);
  EXPECT_EQ(back_button->GetMirroredBounds(), search_icon->GetMirroredBounds());
  EXPECT_LT(clear_button->GetMirroredBounds().right(),
            search_field->width() / 2);
}

}  // namespace
