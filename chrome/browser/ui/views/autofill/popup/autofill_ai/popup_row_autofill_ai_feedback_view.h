// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_POPUP_ROW_AUTOFILL_AI_FEEDBACK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_POPUP_ROW_AUTOFILL_AI_FEEDBACK_VIEW_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageButton;
class View;
}  // namespace views

namespace autofill {
class AutofillPopupController;
}  // namespace autofill

namespace autofill_ai {

// A class for prediction improvements suggestions feedback.
class PopupRowAutofillAiFeedbackView : public autofill::PopupRowView {
  METADATA_HEADER(PopupRowAutofillAiFeedbackView, autofill::PopupRowView)

 public:
  enum class FocusableControl {
    kManageAutofillAiLink,
    kThumbsUp,
    kThumbsDown,
  };

  static constexpr int kLearnMoreStyledLabelViewID = 123;
  static constexpr int kFeedbackTextAndButtonsContainerViewID = 321;

  PopupRowAutofillAiFeedbackView(
      AccessibilitySelectionDelegate& a11y_selection_delegate,
      SelectionDelegate& selection_delegate,
      base::WeakPtr<autofill::AutofillPopupController> controller,
      int line_number);

  PopupRowAutofillAiFeedbackView(const PopupRowAutofillAiFeedbackView&) =
      delete;
  PopupRowAutofillAiFeedbackView& operator=(
      const PopupRowAutofillAiFeedbackView&) = delete;
  ~PopupRowAutofillAiFeedbackView() override;

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

 protected:
  // PopupRowView:
  // Suppresses mouse selection to avoid confusing UX: when selected, the manage
  // predictions link area is highlighted and pressing ENTER opens the link in
  // a new tab. This is supported for accessibility. When a mouse (or other
  // pointing device) is used, there is no need in such highlighting - the link
  // can be just clicked, highlighting the area looks weird.
  // Calls the parent's method unless the selection was triggered by mouse.
  void OnCellSelected(std::optional<CellType> type,
                      autofill::PopupCellSelectionSource source) override;

 private:
  views::View& GetFocusableControlView(FocusableControl focused_control);
  void UpdateFocusedControl(std::optional<FocusableControl> focused_control);

  // The FocusableControl currently focused. Pressing enter will run their
  // respective controller method.s
  std::optional<FocusableControl> focused_control_;
  raw_ptr<views::View> manage_prediction_improvements_link_ = nullptr;
  raw_ptr<views::ImageButton> thumbs_up_button_ = nullptr;
  raw_ptr<views::ImageButton> thumbs_down_button_ = nullptr;
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_POPUP_ROW_AUTOFILL_AI_FEEDBACK_VIEW_H_
