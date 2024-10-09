// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_PREDICTION_IMPROVEMENTS_FEEDBACK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_PREDICTION_IMPROVEMENTS_FEEDBACK_VIEW_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageButton;
}  // namespace views

namespace autofill {

class AutofillPopupController;

// A class for prediction improvements suggestions feedback.
class PopupRowPredictionImprovementsFeedbackView : public PopupRowView {
  METADATA_HEADER(PopupRowPredictionImprovementsFeedbackView, PopupRowView)

 public:
  enum class FocusableControl {
    kManagePredictionImprovementsLink,
    kThumbsUp,
    kThumbsDown,
  };

  static constexpr int kLearnMoreStyledLabelViewID = 123;
  static constexpr int kFeedbackTextAndButtonsContainerViewID = 321;

  PopupRowPredictionImprovementsFeedbackView(
      AccessibilitySelectionDelegate& a11y_selection_delegate,
      SelectionDelegate& selection_delegate,
      base::WeakPtr<AutofillPopupController> controller,
      int line_number);

  PopupRowPredictionImprovementsFeedbackView(
      const PopupRowPredictionImprovementsFeedbackView&) = delete;
  PopupRowPredictionImprovementsFeedbackView& operator=(
      const PopupRowPredictionImprovementsFeedbackView&) = delete;
  ~PopupRowPredictionImprovementsFeedbackView() override;

  views::ImageButton* GetThumbsUpButtonForTest() { return thumbs_up_button_; }
  views::ImageButton* GetThumbsDownButtonForTest() {
    return thumbs_down_button_;
  }

  // PopupRowView:
  void SetSelectedCell(std::optional<CellType> cell) override;
  bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event) override;

  std::optional<FocusableControl> focused_control_for_testing() {
    return focused_control_;
  }

 private:
  void UpdateFocusedControl(std::optional<FocusableControl> focused_control);

  // The FocusableControl currently focused. Pressing enter will run their
  // respective controller method.s
  std::optional<FocusableControl> focused_control_;
  raw_ptr<views::ImageButton> thumbs_up_button_ = nullptr;
  raw_ptr<views::ImageButton> thumbs_down_button_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_PREDICTION_IMPROVEMENTS_FEEDBACK_VIEW_H_
