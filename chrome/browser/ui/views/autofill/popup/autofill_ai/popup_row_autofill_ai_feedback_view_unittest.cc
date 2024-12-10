// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/autofill_ai/popup_row_autofill_ai_feedback_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill_ai {

namespace {

using autofill::AutofillAiSuggestionButtonAction;
using autofill::MockAccessibilitySelectionDelegate;
using autofill::MockAutofillPopupController;
using autofill::MockSelectionDelegate;
using autofill::PopupCellSelectionSource;
using autofill::PopupRowView;
using autofill::PopupViewViews;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::VariantWith;

bool IsA11ySelected(const views::View& view) {
  ui::AXNodeData node_data;
  view.GetViewAccessibility().GetAccessibleNodeData(&node_data);
  return node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

}  // namespace

class PopupRowAutofillAiFeedbackViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    autofill::Suggestion suggestion(
        autofill::SuggestionType::kAutofillAiFeedback);
    suggestion.voice_over = u"Required a11y text";
    controller_.set_suggestions({std::move(suggestion)});
  }

  void ShowView(std::unique_ptr<PopupRowAutofillAiFeedbackView> view) {
    view_ = widget_->SetContentsView(std::move(view));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void CreateFeedbackRowAndGetButtons() {
    auto row = std::make_unique<PopupRowAutofillAiFeedbackView>(
        a11y_selection_delegate(), selection_delegate(),
        controller_.GetWeakPtr(), /*line_number=*/0);
    ShowView(std::move(row));
  }

  // Simulates the keyboard event and returns whether the event was handled.
  bool SimulateKeyPress(int windows_key_code) {
    input::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    event.windows_key_code = windows_key_code;
    return view().HandleKeyPressEvent(event);
  }

 protected:
  MockAutofillPopupController& controller() { return controller_; }
  MockAccessibilitySelectionDelegate& a11y_selection_delegate() {
    return mock_a11y_selection_delegate_;
  }
  MockSelectionDelegate& selection_delegate() {
    return mock_selection_delegate_;
  }
  ui::test::EventGenerator& generator() { return *generator_; }
  PopupRowAutofillAiFeedbackView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  NiceMock<MockAccessibilitySelectionDelegate> mock_a11y_selection_delegate_;
  NiceMock<MockAutofillPopupController> controller_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  NiceMock<MockSelectionDelegate> mock_selection_delegate_;
  raw_ptr<PopupRowAutofillAiFeedbackView> view_ = nullptr;
};

TEST_F(PopupRowAutofillAiFeedbackViewTest,
       ThumbsUpButtonClickTriggersCallback) {
  CreateFeedbackRowAndGetButtons();

  view().SetSelectedCell(PopupRowView::CellType::kContent);

  // Assert thumbs up button callback is run when clicked.
  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(
                  /*line_number=*/0,
                  VariantWith<AutofillAiSuggestionButtonAction>(
                      AutofillAiSuggestionButtonAction::kThumbsUpClicked)));
  // In test env we have to manually set the bounds when a view becomes visible.
  generator().MoveMouseTo(
      view().GetThumbsUpButtonForTest()->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

TEST_F(PopupRowAutofillAiFeedbackViewTest,
       ThumbsDownButtonClickTriggersCallback) {
  CreateFeedbackRowAndGetButtons();

  view().SetSelectedCell(PopupRowView::CellType::kContent);

  // Assert thumbs down button callback is run when clicked.
  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(
                  /*line_number=*/0,
                  VariantWith<AutofillAiSuggestionButtonAction>(
                      AutofillAiSuggestionButtonAction::kThumbsDownClicked)));
  generator().MoveMouseTo(
      view().GetThumbsDownButtonForTest()->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

TEST_F(PopupRowAutofillAiFeedbackViewTest, MouseSelectionIsSuppressed) {
  EXPECT_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck())
      .WillOnce(Return(true));
  CreateFeedbackRowAndGetButtons();

  EXPECT_CALL(selection_delegate(),
              SetSelectedCell(std::make_optional(PopupViewViews::CellIndex{
                                  0, PopupRowView::CellType::kContent}),
                              PopupCellSelectionSource::kMouse))
      .Times(0);
  generator().MoveMouseTo(
      view().GetContentView().GetBoundsInScreen().CenterPoint());
}

TEST_F(PopupRowAutofillAiFeedbackViewTest, FocusTriggersSelectionDelegate) {
  CreateFeedbackRowAndGetButtons();

  EXPECT_CALL(selection_delegate(),
              SetSelectedCell(std::make_optional(PopupViewViews::CellIndex{
                                  0, PopupRowView::CellType::kContent}),
                              PopupCellSelectionSource::kKeyboard));
  view().OnViewFocused(&view().GetContentView());
}

TEST_F(PopupRowAutofillAiFeedbackViewTest, LearnMoreClickTriggersCallback) {
  CreateFeedbackRowAndGetButtons();
  view().SetSelectedCell(PopupRowView::CellType::kContent);

  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(
                  /*line_number=*/0,
                  VariantWith<AutofillAiSuggestionButtonAction>(
                      AutofillAiSuggestionButtonAction::kLearnMoreClicked)));

  auto* suggestion_text =
      views::AsViewClass<views::StyledLabel>(view().GetViewByID(
          PopupRowAutofillAiFeedbackView::kLearnMoreStyledLabelViewID));
  suggestion_text->ClickFirstLinkForTesting();
}

TEST_F(PopupRowAutofillAiFeedbackViewTest,
       LinkIsDefaultFocusableControlForSelectedRow) {
  CreateFeedbackRowAndGetButtons();

  EXPECT_EQ(view().focused_control_for_testing(), std::nullopt);
  view().SetSelectedCell(PopupRowView::CellType::kContent);
  EXPECT_EQ(
      view().focused_control_for_testing(),
      PopupRowAutofillAiFeedbackView::FocusableControl::kManageAutofillAiLink);

  view().SetSelectedCell(std::nullopt);
  EXPECT_EQ(view().focused_control_for_testing(), std::nullopt);
}

TEST_F(PopupRowAutofillAiFeedbackViewTest, LeftRightKeysUpdateFocusedControl) {
  CreateFeedbackRowAndGetButtons();
  view().SetSelectedCell(PopupRowView::CellType::kContent);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().focused_control_for_testing(),
            PopupRowAutofillAiFeedbackView::FocusableControl::kThumbsUp);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().focused_control_for_testing(),
            PopupRowAutofillAiFeedbackView::FocusableControl::kThumbsDown);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(
      view().focused_control_for_testing(),
      PopupRowAutofillAiFeedbackView::FocusableControl::kManageAutofillAiLink)
      << "The list of controls wraps.";

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().focused_control_for_testing(),
            PopupRowAutofillAiFeedbackView::FocusableControl::kThumbsDown);
}

TEST_F(PopupRowAutofillAiFeedbackViewTest,
       LeftRightKeysUpdateFocusedControlRTL) {
  base::i18n::SetRTLForTesting(true);

  CreateFeedbackRowAndGetButtons();
  view().SetSelectedCell(PopupRowView::CellType::kContent);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().focused_control_for_testing(),
            PopupRowAutofillAiFeedbackView::FocusableControl::kThumbsUp);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().focused_control_for_testing(),
            PopupRowAutofillAiFeedbackView::FocusableControl::kThumbsDown);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(
      view().focused_control_for_testing(),
      PopupRowAutofillAiFeedbackView::FocusableControl::kManageAutofillAiLink)
      << "The list of controls wraps.";

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().focused_control_for_testing(),
            PopupRowAutofillAiFeedbackView::FocusableControl::kThumbsDown);
  base::i18n::SetRTLForTesting(false);
}

TEST_F(PopupRowAutofillAiFeedbackViewTest,
       EnterIsHandledForFocusedManageAutofillAiLink) {
  CreateFeedbackRowAndGetButtons();
  view().SetSelectedCell(PopupRowView::CellType::kContent);

  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(
                  /*index=*/0,
                  VariantWith<AutofillAiSuggestionButtonAction>(
                      AutofillAiSuggestionButtonAction::kLearnMoreClicked)));

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RETURN));
}

TEST_F(PopupRowAutofillAiFeedbackViewTest, EnterIsHandledForFocusedThumbsUp) {
  CreateFeedbackRowAndGetButtons();
  view().SetSelectedCell(PopupRowView::CellType::kContent);

  SimulateKeyPress(ui::VKEY_RIGHT);
  ASSERT_EQ(view().focused_control_for_testing(),
            PopupRowAutofillAiFeedbackView::FocusableControl::kThumbsUp);

  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(
                  /*index=*/0,
                  VariantWith<AutofillAiSuggestionButtonAction>(
                      AutofillAiSuggestionButtonAction::kThumbsUpClicked)));

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RETURN));
}

TEST_F(PopupRowAutofillAiFeedbackViewTest, EnterIsHandledForFocusedThumbsDown) {
  CreateFeedbackRowAndGetButtons();
  view().SetSelectedCell(PopupRowView::CellType::kContent);

  SimulateKeyPress(ui::VKEY_RIGHT);
  SimulateKeyPress(ui::VKEY_RIGHT);
  ASSERT_EQ(view().focused_control_for_testing(),
            PopupRowAutofillAiFeedbackView::FocusableControl::kThumbsDown);

  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(
                  /*index=*/0,
                  VariantWith<AutofillAiSuggestionButtonAction>(
                      AutofillAiSuggestionButtonAction::kThumbsDownClicked)));

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RETURN));
}

TEST_F(PopupRowAutofillAiFeedbackViewTest, A11yFocusIsSetToFocusedControl) {
  CreateFeedbackRowAndGetButtons();
  MockFunction<void()> check;
  {
    InSequence s;

    // Default content selection is not suppressed, but it makes no difference
    // to the UX.
    EXPECT_CALL(a11y_selection_delegate(),
                NotifyAXSelection(Ref(view().GetContentView())));
    EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection(Ref(view())));
    EXPECT_CALL(check, Call);

    EXPECT_CALL(a11y_selection_delegate(),
                NotifyAXSelection(Ref(*view().GetThumbsUpButtonForTest())));
    EXPECT_CALL(check, Call);

    EXPECT_CALL(a11y_selection_delegate(),
                NotifyAXSelection(Ref(*view().GetThumbsDownButtonForTest())));
    EXPECT_CALL(check, Call);

    EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection(Ref(view())));
    EXPECT_CALL(check, Call);
  }

  view().SetSelectedCell(PopupRowView::CellType::kContent);
  check.Call();
  EXPECT_TRUE(IsA11ySelected(view()));

  SimulateKeyPress(ui::VKEY_RIGHT);
  check.Call();
  EXPECT_FALSE(IsA11ySelected(view()));
  EXPECT_TRUE(IsA11ySelected(*view().GetThumbsUpButtonForTest()));

  SimulateKeyPress(ui::VKEY_RIGHT);
  check.Call();
  EXPECT_FALSE(IsA11ySelected(*view().GetThumbsUpButtonForTest()));
  EXPECT_TRUE(IsA11ySelected(*view().GetThumbsDownButtonForTest()));

  SimulateKeyPress(ui::VKEY_RIGHT);
  check.Call();
  EXPECT_FALSE(IsA11ySelected(*view().GetThumbsDownButtonForTest()));
  EXPECT_TRUE(IsA11ySelected(view()));
}
}  // namespace autofill_ai
