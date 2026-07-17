// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {

namespace {
constexpr int kNoticePosition = 0;

class PopupPersonalContextNoticeViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    controller_.set_suggestions({SuggestionType::kPersonalContextNotice});
  }

  void ShowView() {
    view_ = widget_->SetContentsView(
        std::make_unique<PopupPersonalContextNoticeView>(
            mock_a11y_selection_delegate_, controller().GetWeakPtr(),
            kNoticePosition));

    // Assign manual bounds so the widget has a physical size.
    // In test env, this is required to position child views
    // so the EventGenerator can accurately click them.
    widget_->SetBounds(gfx::Rect(0, 0, 500, 500));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  PopupPersonalContextNoticeView& view() { return *view_; }
  MockAutofillPopupController& controller() { return controller_; }
  views::Widget& widget() { return *widget_; }
  ui::test::EventGenerator& generator() { return *generator_; }

  testing::AssertionResult VerifyAllLinkBordersFocused(bool expected_focused) {
    bool has_link = false;
    // A multi-line link created by `StyledLabel` is split into multiple link
    // fragments. We check all `views::Link` child views to ensure the focus
    // border highlights the entire wrapped link.
    for (views::View* child : view().description_for_testing()->children()) {
      if (views::IsViewClass<views::Link>(child)) {
        has_link = true;
        views::Link* link = views::AsViewClass<views::Link>(child);
        if (!link->GetBorder()) {
          return testing::AssertionFailure() << "Link border is null.";
        }
        bool actual_focused =
            link->GetBorder()->color() == ui::kColorFocusableBorderFocused;
        if (actual_focused != expected_focused) {
          return testing::AssertionFailure()
                 << "Expected link border focused=" << expected_focused
                 << ", but got " << actual_focused;
        }
      }
    }
    if (!has_link) {
      return testing::AssertionFailure() << "No views::Link child found.";
    }
    return testing::AssertionSuccess();
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  testing::NiceMock<MockAutofillPopupController> controller_;
  testing::NiceMock<MockAccessibilitySelectionDelegate>
      mock_a11y_selection_delegate_;
  raw_ptr<PopupPersonalContextNoticeView> view_ = nullptr;
};

// Tests the initial notice view elements for Ambient Autofill filling source.
TEST_F(PopupPersonalContextNoticeViewTest, InitialStateOnAmbientAutofill) {
  ShowView();

  std::u16string expected_title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_TITLE);
  std::u16string expected_context = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_CONTEXT);
  std::u16string expected_link = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_LINK_TEXT);
  std::u16string expected_ok = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_OK_BUTTON);

  // Check that the description is visible and has the correct text.
  views::StyledLabel* description = view().description_for_testing();
  ASSERT_NE(description, nullptr);
  EXPECT_TRUE(description->GetVisible());
  EXPECT_EQ(
      base::JoinString({expected_title, expected_context, expected_link}, u" "),
      description->GetText());

  // Check that the description contains a link with a correct text.
  views::Link* settings_link = description->GetFirstLinkForTesting();
  EXPECT_TRUE(settings_link);
  EXPECT_EQ(expected_link, settings_link->GetText());

  // Check that the "Got it" button is visible and has the correct text.
  views::MdTextButton* got_it_button = view().got_it_button_for_testing();
  ASSERT_NE(got_it_button, nullptr);
  EXPECT_TRUE(got_it_button->GetVisible());
  EXPECT_EQ(expected_ok, got_it_button->GetText());
}

// Tests the initial notice view elements for AtMemory filling source.
TEST_F(PopupPersonalContextNoticeViewTest, InitialStateAtMemorySource) {
  ON_CALL(controller(), GetMainFillingProduct())
      .WillByDefault(testing::Return(FillingProduct::kAtMemory));

  ShowView();

  std::u16string expected_title = l10n_util::GetStringUTF16(
      IDS_AT_MEMORY_POPUP_PERSONAL_CONTEXT_NOTICE_TITLE);
  std::u16string expected_context = l10n_util::GetStringUTF16(
      IDS_AT_MEMORY_POPUP_PERSONAL_CONTEXT_NOTICE_CONTEXT);
  std::u16string expected_link = l10n_util::GetStringUTF16(
      IDS_AT_MEMORY_POPUP_PERSONAL_CONTEXT_NOTICE_LINK_TEXT);
  std::u16string expected_ok = l10n_util::GetStringUTF16(
      IDS_AT_MEMORY_POPUP_PERSONAL_CONTEXT_NOTICE_OK_BUTTON);

  // Check that the description is visible and has the correct text.
  views::StyledLabel* description = view().description_for_testing();
  ASSERT_NE(description, nullptr);
  EXPECT_TRUE(description->GetVisible());
  EXPECT_EQ(
      base::JoinString({expected_title, expected_context, expected_link}, u" "),
      description->GetText());

  // Check that the description contains a link with a correct text and
  // underline style.
  views::Link* settings_link = description->GetFirstLinkForTesting();
  EXPECT_TRUE(settings_link);
  EXPECT_EQ(expected_link, settings_link->GetText());
  EXPECT_NE(settings_link->font_list().GetFontStyle() & gfx::Font::UNDERLINE,
            0);

  // Check that the "Got it" button is visible and has the correct text.
  views::MdTextButton* got_it_button = view().got_it_button_for_testing();
  ASSERT_NE(got_it_button, nullptr);
  EXPECT_TRUE(got_it_button->GetVisible());
  EXPECT_EQ(expected_ok, got_it_button->GetText());
}

// Tests that clicking on GotIt button triggers the removal of the notice.
TEST_F(PopupPersonalContextNoticeViewTest,
       GotItButtonTriggersRemoveSuggestion) {
  ShowView();

  // Ensure the child views (e.g. got_it_button) are laid out in the widget.
  // This calculates their screen coordinates so they can be accurately
  // located and clicked by the EventGenerator.
  widget().LayoutRootViewIfNecessary();

  views::MdTextButton* got_it_button = view().got_it_button_for_testing();

  EXPECT_CALL(
      controller(),
      RemoveSuggestion(
          kNoticePosition,
          AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked))
      .WillOnce(testing::Return(true));

  generator().MoveMouseTo(got_it_button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

// Tests that clicking the settings link triggers `OnSettingsLinkClicked`.
TEST_F(PopupPersonalContextNoticeViewTest, ClickSettingsLink) {
  ShowView();

  // Ensure the child views are laid out in the widget.
  widget().LayoutRootViewIfNecessary();

  views::StyledLabel* description = view().description_for_testing();
  views::Link* settings_link = description->GetFirstLinkForTesting();
  ASSERT_NE(settings_link, nullptr);

  // The link must not be natively focusable so that it does not steal focus
  // from the search bar/input field.
  EXPECT_EQ(settings_link->GetFocusBehavior(),
            views::View::FocusBehavior::NEVER);

  // Since `chrome::ShowSettingsSubPageForProfile` is not mockable, we verify
  // that `OnSettingsLinkClicked` was triggered by checking the only other thing
  // in the method - that the controller was queried for its WebContents.
  EXPECT_CALL(controller(), GetWebContents()).Times(testing::AtLeast(1));

  generator().MoveMouseTo(settings_link->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

// Tests that when navigation focus enters the notice view's row, it lands on
// the "Settings" link.
TEST_F(PopupPersonalContextNoticeViewTest, NavigateToNoticeView) {
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().got_it_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());
  EXPECT_EQ(view().GetSelectedCell(),
            PopupInteractiveRowView::CellType::kContent);

  // `SetSelectedCell` moves the navigation to the given cell.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);

  // Verify the correct element ("Settings" link) is focused.
  EXPECT_TRUE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  // Verify the correct element ("Settings" link) has the visual treatment.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus is inside of the notice view, the RIGHT/LEFT
// keyboard navigation moves it between "Settings" link and "Got it" button in
// LTR.
TEST_F(PopupPersonalContextNoticeViewTest, NavigateInsideOfNoticeView) {
  base::i18n::SetRTLForTesting(false);
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().got_it_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  // The initial state: the focus is on the "Settings" link.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(view().is_link_focused_for_testing());
  ASSERT_FALSE(view().is_button_focused_for_testing());

  // Navigate to the right.
  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));

  // Verify the focus moved from the "Settings" link to the "Got it" button.
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_TRUE(view().is_button_focused_for_testing());

  // Verify the "Settings" link border is reset.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  // Verify the focus ring on the "Got it" button is painted.
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_HOVERED);
  EXPECT_TRUE(button_focus_ring->ShouldPaintForTesting());

  // Navigate to the left.
  input::NativeWebKeyboardEvent left_event = right_event;
  left_event.windows_key_code = ui::VKEY_LEFT;
  EXPECT_TRUE(view().HandleKeyPressEvent(left_event));

  // Verify the focus moved from the "Got it" button to the "Settings" link.
  EXPECT_TRUE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  // Verify the "Settings" link border is focused again.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  // Verify the focus ring on the "Got it" button is no longer painted.
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus is inside of the notice view, the RIGHT/LEFT
// keyboard navigation moves it between "Settings" link and "Got it" button in
// RTL.
TEST_F(PopupPersonalContextNoticeViewTest, NavigateInsideOfNoticeViewRTL) {
  base::i18n::SetRTLForTesting(true);
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().got_it_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  // The initial state: the focus is on the "Settings" link.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(view().is_link_focused_for_testing());
  ASSERT_FALSE(view().is_button_focused_for_testing());

  // Navigate to the left (moves towards the button in RTL).
  input::NativeWebKeyboardEvent left_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  left_event.windows_key_code = ui::VKEY_LEFT;
  EXPECT_TRUE(view().HandleKeyPressEvent(left_event));

  // Verify the focus moved from the "Settings" link to the "Got it" button.
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_TRUE(view().is_button_focused_for_testing());

  // Verify the "Settings" link border is reset.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  // Verify the focus ring on the "Got it" button is painted.
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_HOVERED);
  EXPECT_TRUE(button_focus_ring->ShouldPaintForTesting());

  // Navigate to the right (moves towards the link in RTL).
  input::NativeWebKeyboardEvent right_event = left_event;
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));

  // Verify the focus moved from the "Got it" button to the "Settings" link.
  EXPECT_TRUE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  // Verify the "Settings" link border is focused again.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  // Verify the focus ring on the "Got it" button is no longer painted.
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());

  base::i18n::SetRTLForTesting(false);
}

// Tests that when navigation focus leaves the notice view through the
// "Settings" link, no notice element is focused.
TEST_F(PopupPersonalContextNoticeViewTest, NavigateFromNoticeViewLink) {
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().got_it_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());
  EXPECT_EQ(view().GetSelectedCell(),
            PopupInteractiveRowView::CellType::kContent);

  // The initial state: the focus is on the "Settings" link.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  EXPECT_TRUE(view().is_link_focused_for_testing());

  // `SetSelectedCell` with empty `CellType` removes the focus from the cell.
  view().SetSelectedCell(std::nullopt);

  // Verify no element is focused.
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  // Verify no element has the visual focus treatment.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus leaves the notice view through the
// "Got it" button, no notice element is focused.
TEST_F(PopupPersonalContextNoticeViewTest, NavigateFromNoticeViewButton) {
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().got_it_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());
  EXPECT_EQ(view().GetSelectedCell(),
            PopupInteractiveRowView::CellType::kContent);

  // The initial state: the focus is on the "Got it" button.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));
  EXPECT_TRUE(view().is_button_focused_for_testing());

  // `SetSelectedCell` with empty `CellType` removes the focus from the cell.
  view().SetSelectedCell(std::nullopt);

  // Verify no element is focused.
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  // Verify no element has the visual focus treatment.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

}  // namespace
}  // namespace autofill
