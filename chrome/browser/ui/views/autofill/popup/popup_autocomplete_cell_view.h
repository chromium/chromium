// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_AUTOCOMPLETE_CELL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_AUTOCOMPLETE_CELL_VIEW_H_

#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"

namespace content {
struct NativeWebKeyboardEvent;
}  // namespace content

namespace views {
class ImageButton;
}  // namespace views

namespace autofill {
class AutofillPopupController;
}

namespace autofill {
// `PopupAutocompleteCellView` represents a single, selectable cell. However, It
// contains the autocomplete value AND a button to delete the entry.
class PopupAutocompleteCellView : public autofill::PopupCellView {
 public:
  PopupAutocompleteCellView(base::WeakPtr<AutofillPopupController> controller,
                            int line_number);
  PopupAutocompleteCellView(const PopupAutocompleteCellView&) = delete;
  PopupAutocompleteCellView& operator=(const PopupAutocompleteCellView&) =
      delete;
  ~PopupAutocompleteCellView() override;

  // autofill::PopupCellView;
  void SetSelected(bool selected) override;
  // Handles key press event coming from the parent class. Returns false if
  // parent should handle it.
  bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event) override;

  views::ImageButton* GetDeleteButton();

 private:
  void CreateDeleteButton();
  void OnButtonPropertyChanged();
  void DeleteAutocomplete();
  void UpdateSelectedAndRunCallback(bool selected);
  void HandleKeyPressEventFocusOnButton();
  void HandleKeyPressEventFocusOnContent();

  raw_ptr<views::ImageButton> button_;
  base::CallbackListSubscription button_state_change_subscription_;
  // The controller for the parent view.
  const base::WeakPtr<AutofillPopupController> controller_;
  // The line number in the popup.
  const int line_number_;
  // Whether the button has been focused. Used for accessibility and arrow
  // navigation purposes.
  bool button_focused_ = false;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_AUTOCOMPLETE_CELL_VIEW_H_
