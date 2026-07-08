// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/md_text_button.h"
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
    auto content_view = std::make_unique<PopupRowContentView>();
    view_ = widget_->SetContentsView(
        std::make_unique<PopupPersonalContextNoticeView>(
            a11y_selection_delegate(), selection_delegate(),
            controller().GetWeakPtr(), kNoticePosition,
            std::move(content_view)));

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
  MockAccessibilitySelectionDelegate& a11y_selection_delegate() {
    return mock_a11y_selection_delegate_;
  }
  MockSelectionDelegate& selection_delegate() {
    return mock_selection_delegate_;
  }
  PopupPersonalContextNoticeView& view() { return *view_; }
  MockAutofillPopupController& controller() { return controller_; }
  views::Widget& widget() { return *widget_; }
  ui::test::EventGenerator& generator() { return *generator_; }

 private:
  testing::NiceMock<MockAccessibilitySelectionDelegate>
      mock_a11y_selection_delegate_;
  testing::NiceMock<MockSelectionDelegate> mock_selection_delegate_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  testing::NiceMock<MockAutofillPopupController> controller_;
  raw_ptr<PopupPersonalContextNoticeView> view_ = nullptr;
};

// Tests the notice view is correctly created and displays its initial elements.
TEST_F(PopupPersonalContextNoticeViewTest, InitialState) {
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

}  // namespace
}  // namespace autofill
