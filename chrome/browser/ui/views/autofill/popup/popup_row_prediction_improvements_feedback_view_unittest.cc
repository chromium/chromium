// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_prediction_improvements_feedback_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {

using ::testing::NiceMock;
using ::testing::VariantWith;

class PopupRowPredictionImprovementsFeedbackViewTest
    : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    controller_.set_suggestions(
        {Suggestion(SuggestionType::kPredictionImprovementsFeedback)});
  }

  void ShowView(
      std::unique_ptr<PopupRowPredictionImprovementsFeedbackView> view) {
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
    auto row = std::make_unique<PopupRowPredictionImprovementsFeedbackView>(
        a11y_selection_delegate(), selection_delegate(),
        controller_.GetWeakPtr(), /*line_number=*/0);
    ShowView(std::move(row));
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
  PopupRowPredictionImprovementsFeedbackView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  NiceMock<MockAccessibilitySelectionDelegate> mock_a11y_selection_delegate_;
  NiceMock<MockAutofillPopupController> controller_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  NiceMock<MockSelectionDelegate> mock_selection_delegate_;
  raw_ptr<PopupRowPredictionImprovementsFeedbackView> view_ = nullptr;
};

TEST_F(PopupRowPredictionImprovementsFeedbackViewTest,
       ThumbsUpButtonClickTriggersCallback) {
  CreateFeedbackRowAndGetButtons();

  view().SetSelectedCell(PopupRowView::CellType::kContent);

  // Assert thumbs up button callback is run when clicked.
  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(
                  /*line_number=*/0,
                  VariantWith<PredictionImprovementsButtonActions>(
                      PredictionImprovementsButtonActions::kThumbsUpClicked)));
  // In test env we have to manually set the bounds when a view becomes visible.
  generator().MoveMouseTo(
      view().GetThumbsUpButtonForTest()->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

TEST_F(PopupRowPredictionImprovementsFeedbackViewTest,
       ThumbsDownButtonClickTriggersCallback) {
  CreateFeedbackRowAndGetButtons();

  view().SetSelectedCell(PopupRowView::CellType::kContent);

  // Assert thumbs down button callback is run when clicked.
  EXPECT_CALL(
      controller(),
      PerformButtonActionForSuggestion(
          /*line_number=*/0,
          VariantWith<PredictionImprovementsButtonActions>(
              PredictionImprovementsButtonActions::kThumbsDownClicked)));
  generator().MoveMouseTo(
      view().GetThumbsDownButtonForTest()->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

TEST_F(PopupRowPredictionImprovementsFeedbackViewTest,
       LearnMoreClickTriggersCallback) {
  CreateFeedbackRowAndGetButtons();
  view().SetSelectedCell(PopupRowView::CellType::kContent);

  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(
                  /*line_number=*/0,
                  VariantWith<PredictionImprovementsButtonActions>(
                      PredictionImprovementsButtonActions::kLearnMoreClicked)));

  auto* suggestion_text = views::AsViewClass<
      views::StyledLabel>(view().GetViewByID(
      PopupRowPredictionImprovementsFeedbackView::kLearnMoreStyledLabelViewID));
  suggestion_text->ClickFirstLinkForTesting();
}

}  // namespace autofill
