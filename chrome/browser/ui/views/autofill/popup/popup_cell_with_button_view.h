// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_WITH_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_WITH_BUTTON_VIEW_H_

#include <memory>
#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view_observer.h"

namespace content {
struct NativeWebKeyboardEvent;
}  // namespace content

namespace views {
class ImageButton;
}  // namespace views

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

// A class for a single selectable popup cell that also has a button.
class PopupCellWithButtonView : public PopupCellView,
                                public CellButtonDelegate {
 public:
  METADATA_HEADER(PopupCellWithButtonView);
  PopupCellWithButtonView(base::WeakPtr<AutofillPopupController> controller,
                          int line_number);

  PopupCellWithButtonView(const PopupCellWithButtonView&) = delete;
  PopupCellWithButtonView& operator=(const PopupCellWithButtonView&) = delete;
  ~PopupCellWithButtonView() override;

  // Removes the current button (if there is any) and adds `cell_button` to the
  // view. Note that `cell_button` is appended to the current children of `this`
  // and its controller is overwritten.
  void SetCellButton(std::unique_ptr<views::ImageButton> cell_button);
  views::ImageButton* GetCellButtonForTest() { return button_; }
  bool GetCellButtonFocusedForTest() { return button_focused_; }

  // Determines under which conditions the button (if there is one) is visible.
  enum class CellButtonBehavior {
    // The button is only visible if the cell or the button are selected or
    // hovered.
    kShowOnHoverOrSelect,
    // The button is always visible.
    kShowAlways,
  };
  // Sets the visibility behavior of the cell button.
  void SetCellButtonBehavior(CellButtonBehavior cell_button_behavior);

  // Returns the view that contains the button or `nullptr` if no button is set.
  views::View* GetButtonContainer() { return button_placeholder_; }

  // PopupCellView:
  void SetSelected(bool selected) override;
  // Handles key press events coming from the parent class. Returns false if
  // the parent should handle it.
  bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event) override;

 private:
  // CellButtonDelegate:
  void OnMouseEnteredCellButton() override;
  void OnMouseExitedCellButton() override;

  void UpdateSelectedAndRunCallback(bool selected);
  void HandleKeyPressEventFocusOnButton();
  void HandleKeyPressEventFocusOnContent();

  // Returns whether the cell button (if there is one) should be visible.
  bool ShouldCellButtonBeVisible() const;

  // TODO(crbug.com/1491373): Make it inherited from PopupRowView, remove
  // `controller_` and `line_number_`, and use them from the parent class.
  const base::WeakPtr<AutofillPopupController> controller_;
  const int line_number_;

  raw_ptr<views::ImageButton> button_ = nullptr;
  raw_ptr<ButtonPlaceholder> button_placeholder_ = nullptr;

  // Whether the button has been focused. Used for accessibility and arrow
  // navigation purposes.
  bool button_focused_ = false;

  CellButtonBehavior cell_button_behavior_ =
      CellButtonBehavior::kShowOnHoverOrSelect;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_WITH_BUTTON_VIEW_H_
