// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace content {
struct NativeWebKeyboardEvent;
}  // namespace content

namespace autofill {

class AutofillPopupController;
class PopupCellView;
class PopupRowStrategy;
class PopupViewViews;

// `PopupRowView` represents a single selectable popup row. Different styles
// of the row can be achieved by injecting the respective `PopupRowStrategy`
// objects in the constructor.
class PopupRowView : public views::View {
 public:
  // Enum class describing the different cells that a `PopupRowView` can
  // contain.
  enum class CellType {
    // The cell containing the main content of the row.
    kContent = 0,
    // The cell containing the control elements (such as a delete button).
    kControl = 1
  };

  // Interface used to announce changes in selected cells to accessibility
  // frameworks.
  class AccessibilitySelectionDelegate {
   public:
    virtual ~AccessibilitySelectionDelegate() = default;

    // Notify accessibility that an item represented by `view` has been
    // selected.
    virtual void NotifyAXSelection(views::View& view) = 0;
  };

  // Interface used to keep track of cell selection. This may be needed if the
  // parent needs to keep state of which row is currently in order to process
  // keyboard events.
  class SelectionDelegate {
   public:
    using CellIndex = std::pair<size_t, PopupRowView::CellType>;

    virtual ~SelectionDelegate() = default;

    virtual absl::optional<CellIndex> GetSelectedCell() const = 0;
    virtual void SetSelectedCell(absl::optional<CellIndex> cell_index) = 0;
  };

  METADATA_HEADER(PopupRowView);
  PopupRowView(AccessibilitySelectionDelegate& a11y_selection_delegate,
               SelectionDelegate& selection_delegate,
               base::WeakPtr<AutofillPopupController> controller,
               std::unique_ptr<PopupRowStrategy> strategy);
  PopupRowView(const PopupRowView&) = delete;
  PopupRowView& operator=(const PopupRowView&) = delete;
  ~PopupRowView() override;

  // Acts as a factory method for creating a row view.
  static std::unique_ptr<PopupRowView> Create(PopupViewViews& popup_view,
                                              int line_number);

  // Gets and sets the selected cell within this row.
  absl::optional<CellType> GetSelectedCell() const { return selected_cell_; }
  void SetSelectedCell(absl::optional<CellType> cell);

  // Sets the highlighted state on the cell of specified type.
  void SetCellPermanentlyHighlighted(CellType cell, bool highlighted);

  // Returns the cell's bounds, the cell of the requested type must be present.
  gfx::RectF GetCellBounds(CellType cell) const;

  // Attempts to process a key press `event`. Returns true if it did (and the
  // parent no longer needs to handle it).
  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event);

  // Returns the view representing the content area of the row.
  PopupCellView& GetContentView() { return *content_view_; }
  // Returns the view representing the control area of the row. Can be null.
  PopupCellView* GetControlView() { return control_view_.get(); }

 private:
  // Selects the next/previous cell, if there is one. Otherwise leaves the
  // current selection. Does not wrap.
  void SelectNextCell();
  void SelectPreviousCell();

  // Returns the cell view or `nullptr` if it was not created.
  const PopupCellView* GetCellView(CellType type) const;
  PopupCellView* GetCellView(CellType type);

  AccessibilitySelectionDelegate& GetA11ySelectionDelegate() {
    return a11y_selection_delegate_.get();
  }

  // The delegate used for accessibility announcements (implemented by the
  // parent).
  const raw_ref<AccessibilitySelectionDelegate> a11y_selection_delegate_;
  // The controller for the parent view.
  const base::WeakPtr<AutofillPopupController> controller_;
  // The strategy from which the actual view content of this row is created.
  const std::unique_ptr<PopupRowStrategy> strategy_;

  // Which (if any) cell of this row is currently selected.
  absl::optional<CellType> selected_cell_;

  // The cell wrapping the content area of the row.
  raw_ptr<PopupCellView> content_view_ = nullptr;
  // The cell wrapping the control area of the row.
  // TODO(crbug.com/1411172): Add keyboard event handling.
  raw_ptr<PopupCellView> control_view_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_
