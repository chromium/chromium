// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_WITH_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_WITH_BUTTON_VIEW_H_

#include <memory>
#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view_observer.h"

namespace content {
struct NativeWebKeyboardEvent;
}  // namespace content

namespace views {
class ImageButton;
}  // namespace views

namespace autofill {

// Receives notifications of mouse enter/exit events of the button.
class ButtonDelegate {
 public:
  virtual void OnMouseEnteredButton() = 0;
  virtual void OnMouseExitedButton() = 0;
};

namespace {
class ButtonPlaceholder;
}  // namespace

// A class for a single selectable popup cell that also has a button.
class PopupRowWithButtonView : public PopupRowView, public ButtonDelegate {
 public:
  METADATA_HEADER(PopupRowWithButtonView);

  // Determines under which conditions the button (if there is one) is visible.
  enum class ButtonBehavior {
    // The button is only visible if the cell or the button are selected or
    // hovered.
    kShowOnHoverOrSelect,
    // The button is always visible.
    kShowAlways,
  };

  PopupRowWithButtonView(
      AccessibilitySelectionDelegate& a11y_selection_delegate,
      SelectionDelegate& selection_delegate,
      base::WeakPtr<AutofillPopupController> controller,
      int line_number,
      std::unique_ptr<PopupRowContentView> content_view,
      std::unique_ptr<views::ImageButton> button,
      ButtonBehavior button_behavior);

  PopupRowWithButtonView(const PopupRowWithButtonView&) = delete;
  PopupRowWithButtonView& operator=(const PopupRowWithButtonView&) = delete;
  ~PopupRowWithButtonView() override;

  views::ImageButton* GetButtonForTest() { return button_; }
  bool GetButtonFocusedForTest() {
    return focused_part_ == RowWithButtonPart::kButton;
  }

  // Returns the view that contains the button or `nullptr` if no button is set.
  views::View* GetButtonContainer();

  // PopupRowView:
  void SetSelectedCell(absl::optional<CellType> cell) override;
  // Handles key press events coming from the parent class. Returns false if
  // the parent should handle it.
  bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event) override;

 private:
  // Enumerates logical parts of the row. Used for accessibility and arrow
  // navigation purposes.
  enum class RowWithButtonPart {
    kContent,
    kButton,
  };

  // ButtonDelegate:
  void OnMouseEnteredButton() override;
  void OnMouseExitedButton() override;

  // Sets the `focused_part_` property and calls `SelectSuggestion()` on
  // the controller according to the focused part.
  void UpdateFocusedPartAndSelectedSuggestion(
      absl::optional<RowWithButtonPart> part);
  void HandleKeyPressEventFocusOnButton();
  void HandleKeyPressEventFocusOnContent();

  // Returns whether the button (if there is one) should be visible.
  bool ShouldButtonBeVisible() const;

  raw_ptr<views::ImageButton> button_ = nullptr;
  raw_ptr<ButtonPlaceholder> button_placeholder_ = nullptr;

  // Defines the part of the row that is currently highlighted and accepts
  // user input.
  absl::optional<RowWithButtonPart> focused_part_;

  ButtonBehavior button_behavior_ = ButtonBehavior::kShowOnHoverOrSelect;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_WITH_BUTTON_VIEW_H_
