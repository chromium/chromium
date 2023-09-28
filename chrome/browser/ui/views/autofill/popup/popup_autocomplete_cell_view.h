// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_AUTOCOMPLETE_CELL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_AUTOCOMPLETE_CELL_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "ui/views/view_observer.h"

namespace content {
struct NativeWebKeyboardEvent;
}  // namespace content

namespace views {
class ImageButton;
}  // namespace views

namespace autofill {
class AutofillPopupController;
}

namespace gfx {
class canvas;
}  // namespace gfx

namespace autofill {

// Receives notifications of mouse enter/exit events of the cell button.
class CellButtonDelegate {
 public:
  virtual void OnMouseEnteredCellButton() = 0;
  virtual void OnMouseExitedCellButton() = 0;
};

namespace {

// Used to determine when both the placeholder and the button are painted
// and have dimensions. This is important to solve the issue where deleting an
// entry leads to another entry being rendered right under the cursor.
class ButtonPlaceholder : public views::View, public views::ViewObserver {
 public:
  explicit ButtonPlaceholder(CellButtonDelegate* cell_button_delegate);
  ~ButtonPlaceholder() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  int GetHeightForWidth(int width) const override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

 private:
  // Scoped observation for OnViewBoundsChanged.
  base::ScopedObservation<views::View, views::ViewObserver>
      view_bounds_changed_observer_{this};
  const raw_ptr<CellButtonDelegate> cell_button_delegate_ = nullptr;
  bool first_paint_happened_ = false;
};

}  // namespace

// `PopupAutocompleteCellView` represents a single, selectable cell. However, It
// contains the autocomplete value AND a button to delete the entry.
class PopupAutocompleteCellView : public autofill::PopupCellView,
                                  public CellButtonDelegate {
 public:
  PopupAutocompleteCellView(base::WeakPtr<AutofillPopupController> controller,
                            int line_number);
  PopupAutocompleteCellView(const PopupAutocompleteCellView&) = delete;
  PopupAutocompleteCellView& operator=(const PopupAutocompleteCellView&) =
      delete;
  ~PopupAutocompleteCellView() override;

  // autofill::PopupCellView;
  void SetSelected(bool selected) override;
  // Handles key press events coming from the parent class. Returns false if
  // the parent should handle it.
  bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event) override;

  views::ImageButton* GetCellButtonForTest() { return button_; }

 private:
  // CellButtonDelegate:
  void OnMouseEnteredCellButton() override;
  void OnMouseExitedCellButton() override;

  void CreateDeleteButton();
  void DeleteAutocompleteEntry();
  void UpdateSelectedAndRunCallback(bool selected);
  void HandleKeyPressEventFocusOnButton();
  void HandleKeyPressEventFocusOnContent();

  raw_ptr<views::ImageButton> button_ = nullptr;
  raw_ptr<ButtonPlaceholder> button_placeholder_ = nullptr;

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
