// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "components/autofill/core/common/aliases.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"

namespace views {
class BoxLayoutView;
class ScrollView;
}  // namespace views

namespace autofill {

class AutofillPopupController;
class PopupSeparatorView;
class PopupWarningView;

// Views implementation for the autofill and password suggestion.
class PopupViewViews : public PopupBaseView,
                       public AutofillPopupView,
                       public PopupRowView::SelectionDelegate {
 public:
  METADATA_HEADER(PopupViewViews);

  using RowPointer =
      absl::variant<PopupRowView*, PopupSeparatorView*, PopupWarningView*>;

  PopupViewViews(base::WeakPtr<AutofillPopupController> controller,
                 views::Widget* parent_widget);
  PopupViewViews(const PopupViewViews&) = delete;
  PopupViewViews& operator=(const PopupViewViews&) = delete;
  ~PopupViewViews() override;

  base::WeakPtr<AutofillPopupController> controller() { return controller_; }
  // Gets and sets the currently selected cell. If an invalid `cell_index` is
  // passed, `GetSelectedCell()` will return `absl::nullopt` afterwards.
  absl::optional<CellIndex> GetSelectedCell() const override;
  void SetSelectedCell(absl::optional<CellIndex> cell_index) override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // AutofillPopupView:
  void Show(AutoselectFirstSuggestion autoselect_first_suggestion) override;
  void Hide() override;
  absl::optional<int32_t> GetAxUniqueId() override;
  void AxAnnounce(const std::u16string& text) override;
  base::WeakPtr<AutofillPopupView> GetWeakPtr() override;

  // PopupBaseView:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

 private:
  friend class PopupViewViewsBrowsertest;
  friend class PopupViewViewsTest;

  const std::vector<RowPointer>& GetRowsForTesting() { return rows_; }

  // Returns the `PopupRowView` at line number `index`. Assumes that there is
  // such a view at that line number - otherwise the underlying variant will
  // check false.
  PopupRowView& GetPopupRowViewAt(size_t index) {
    return *absl::get<PopupRowView*>(rows_[index]);
  }
  const PopupRowView& GetPopupRowViewAt(size_t index) const {
    return *absl::get<PopupRowView*>(rows_[index]);
  }

  // Returns whether the row at `index` exists and is a `PopupRowView`.
  bool HasPopupRowViewAt(size_t index) const;

  // Creates child views based on the suggestions given by |controller_|.
  // This method expects that all non-footer suggestions precede footer
  // suggestions.
  void CreateChildViews();

  // Applies certain rounding rules to the given width, such as matching the
  // element width when possible.
  int AdjustWidth(int width) const;

  // Selects the first row prior to the currently selected one that is
  // selectable (e.g. not a separator). If no row is selected or no row prior to
  // the current one is selectable, it tries to select the last row. If that one
  // is unselectable, no row is selected.
  void SelectPreviousRow();

  // Analogous to previous row, just in the opposite direction: Tries to find
  // the next selectable row after the currently selected one. If no row is
  // selected or no row following the currently selected one is selectable, it
  // tries to select the first row. If that one is unselectable, no row is
  // selected.
  void SelectNextRow();

  // Attempts to accept the selected cell. It will return false if the cell is
  // not selectable or the current cell selection is invalid.
  // If `tab_key_pressed` is true, only cells that trigger field filling or
  // scanning a credit card qualify as selectable.
  bool AcceptSelectedCell(bool tab_key_pressed);

  // Attempts to remove the selected cell. Only content cells are allowed to be
  // selected.
  bool RemoveSelectedCell();

  // AutofillPopupView:
  bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void OnSuggestionsChanged() override;

  // PopupBaseView:
  bool DoUpdateBoundsAndRedrawPopup() override;

  // Controller for this view.
  base::WeakPtr<AutofillPopupController> controller_ = nullptr;
  // The index of the row with a selected cell.
  absl::optional<size_t> row_with_selected_cell_;
  std::vector<RowPointer> rows_;
  raw_ptr<views::ScrollView, DanglingUntriaged> scroll_view_ = nullptr;
  raw_ptr<views::BoxLayoutView, DanglingUntriaged> body_container_ = nullptr;

  base::WeakPtrFactory<AutofillPopupView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_H_
