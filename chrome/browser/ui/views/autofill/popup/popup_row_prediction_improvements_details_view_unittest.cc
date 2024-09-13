// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_prediction_improvements_details_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/input/native_web_keyboard_event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {
namespace {

using ::testing::NiceMock;
using ::testing::VariantWith;

class PopupRowPredictionImprovementsDetailsViewTest
    : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    controller_.set_suggestions(
        {Suggestion(SuggestionType::kRetrievePredictionImprovements)});
  }

  void ShowView(
      std::unique_ptr<PopupRowPredictionImprovementsDetailsView> view) {
    view_ = widget_->SetContentsView(std::move(view));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void SimulateKeyPress(int windows_key_code) {
    input::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    event.windows_key_code = windows_key_code;
    view().HandleKeyPressEvent(event);
  }

  void CreatePredictionImprovementsDetailsRow() {
    auto row = std::make_unique<PopupRowPredictionImprovementsDetailsView>(
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
  PopupRowPredictionImprovementsDetailsView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  NiceMock<MockAccessibilitySelectionDelegate> mock_a11y_selection_delegate_;
  NiceMock<MockAutofillPopupController> controller_;
  std::unique_ptr<views::Widget> widget_;
  NiceMock<MockSelectionDelegate> mock_selection_delegate_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  raw_ptr<PopupRowPredictionImprovementsDetailsView> view_ = nullptr;
};

TEST_F(PopupRowPredictionImprovementsDetailsViewTest,
       LearnMoreClickTriggersCallback) {
  CreatePredictionImprovementsDetailsRow();
  view().SetSelectedCell(PopupRowView::CellType::kContent);

  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(
                  /*line_number=*/0,
                  VariantWith<PredictionImprovementsButtonActions>(
                      PredictionImprovementsButtonActions::kLearnMoreClicked)));

  auto* suggestion_text = views::AsViewClass<
      views::StyledLabel>(view().GetViewByID(
      PopupRowPredictionImprovementsDetailsView::kLearnMoreStyledLabelViewID));
  suggestion_text->ClickFirstLinkForTesting();
}

TEST_F(PopupRowPredictionImprovementsDetailsViewTest, KeyPressEnter) {
  CreatePredictionImprovementsDetailsRow();

  // At first the content is selected.
  view().SetSelectedCell(PopupRowView::CellType::kContent);
  EXPECT_EQ(view().GetSelectedCell(), PopupRowView::CellType::kContent);

  // Pressing enter when the link is focused triggers the callback.
  EXPECT_CALL(controller(),
              PerformButtonActionForSuggestion(
                  /*line_number=*/0,
                  VariantWith<PredictionImprovementsButtonActions>(
                      PredictionImprovementsButtonActions::kLearnMoreClicked)));
  SimulateKeyPress(ui::VKEY_RETURN);
}

}  // namespace
}  // namespace autofill
