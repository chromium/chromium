// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_INTERACTIVE_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_INTERACTIVE_ROW_VIEW_H_

#include <optional>

#include "components/input/native_web_keyboard_event.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace autofill {

// A class representing a selectable row in an Autofill popup.
class PopupInteractiveRowView : public views::View {
  METADATA_HEADER(PopupInteractiveRowView, views::View)

 public:
  // Enum class describing the different cells that a `PopupInteractiveRowView`
  // can contain.
  enum class CellType {
    // The cell containing the main content of the row.
    kContent = 0,
    // The cell containing the control elements (such as a delete button).
    kControl = 1
  };

  PopupInteractiveRowView();
  PopupInteractiveRowView(const PopupInteractiveRowView&) = delete;
  PopupInteractiveRowView& operator=(const PopupInteractiveRowView&) = delete;
  ~PopupInteractiveRowView() override;

  // Gets and sets the selected cell within this row.
  virtual std::optional<CellType> GetSelectedCell() const = 0;
  virtual void SetSelectedCell(std::optional<CellType> cell) = 0;

  // Attempts to process a key press `event`. Returns true if it did (and the
  // parent no longer needs to handle it).
  virtual bool HandleKeyPressEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Returns if the popup row is available for selection.
  virtual bool IsSelectable() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_INTERACTIVE_ROW_VIEW_H_
