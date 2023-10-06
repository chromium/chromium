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
#include "base/timer/timer.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "components/autofill/core/common/aliases.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace views {
class BoxLayoutView;
class ScrollView;
}  // namespace views

namespace autofill {

class AutofillPopupController;
class PopupSeparatorView;
class PopupWarningView;

// Sub-popups and their parent popups are connected by providing children
// with links to their parents. This interface defines the API exposed by
// these links.
class ExpandablePopupParentView {
 private:
  friend class PopupViewViews;

  // Callbacks to notify the parent of the children about hover state changes.
  // The calls are also propagated to grandparents, so that, no matter how
  // long the chain of sub-popups is, lower level popups know the hover
  // status in (grand)children.
  virtual void OnMouseEnteredInChildren() = 0;
  virtual void OnMouseExitedInChildren() = 0;
};

// Views implementation for the autofill and password suggestion.
class PopupViewViews : public PopupBaseView,
                       public AutofillPopupView,
                       public PopupRowView::SelectionDelegate,
                       public ExpandablePopupParentView {
 public:
  METADATA_HEADER(PopupViewViews);

  using RowPointer =
      absl::variant<PopupRowView*, PopupSeparatorView*, PopupWarningView*>;

  // The time it takes for a selected cell to open a sub-popup if it has one.
  static constexpr base::TimeDelta kMouseOpenSubPopupDelay =
      base::Milliseconds(250);
  static constexpr base::TimeDelta kNonMouseOpenSubPopupDelay =
      kMouseOpenSubPopupDelay / 10;

  // The delay for closing the sub-popup after having no cell selected,
  // sub-popup cells are also taken into account.
  static constexpr base::TimeDelta kNoSelectionHideSubPopupDelay =
      base::Milliseconds(2500);

  PopupViewViews(base::WeakPtr<AutofillPopupController> controller,
                 base::WeakPtr<ExpandablePopupParentView> parent,
                 views::Widget* parent_widget);
  explicit PopupViewViews(base::WeakPtr<AutofillPopupController> controller);
  PopupViewViews(const PopupViewViews&) = delete;
  PopupViewViews& operator=(const PopupViewViews&) = delete;
  ~PopupViewViews() override;

  base::WeakPtr<AutofillPopupController> controller() { return controller_; }
  // Gets and sets the currently selected cell. If an invalid `cell_index` is
  // passed, `GetSelectedCell()` will return `absl::nullopt` afterwards.
  absl::optional<CellIndex> GetSelectedCell() const override;
  void SetSelectedCell(absl::optional<CellIndex> cell_index,
                       PopupCellSelectionSource source) override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // AutofillPopupView:
  bool Show(AutoselectFirstSuggestion autoselect_first_suggestion) override;
  void Hide() override;
  bool OverlapsWithPictureInPictureWindow() const override;
  absl::optional<int32_t> GetAxUniqueId() override;
  void AxAnnounce(const std::u16string& text) override;
  base::WeakPtr<AutofillPopupView> CreateSubPopupView(
      base::WeakPtr<AutofillPopupController> controller) override;
  std::optional<AutofillClient::PopupScreenLocation> GetPopupScreenLocation()
      const override;
  base::WeakPtr<AutofillPopupView> GetWeakPtr() override;

  // PopupBaseView:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

 private:
  friend class PopupViewViewsTestApi;

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

  // Instantiates the content of the popup.
  void InitViews();

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

  // Selects the next/previous in horizontal direction (i.e. left to right or
  // vice versa) cell, if there is one. Otherwise leaves the current selection.
  // Does not wrap.
  bool SelectNextHorizontalCell();
  bool SelectPreviousHorizontalCell();

  // Attempts to accept the selected cell. It will return false if there is no
  // selected cell or the cell does not trigger field filling or scanning a
  // credit card. `event_time` must be the time the user input event was
  // triggered.
  bool AcceptSelectedContentOrCreditCardCell(base::TimeTicks event_time);

  // Attempts to remove the selected cell. Only content cells are allowed to be
  // selected.
  bool RemoveSelectedCell();

  // AutofillPopupView:
  bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void OnSuggestionsChanged() override;

  // PopupBaseView:
  bool DoUpdateBoundsAndRedrawPopup() override;

  // ExpandablePopupParentView:
  void OnMouseEnteredInChildren() override;
  void OnMouseExitedInChildren() override;

  bool CanShowDropdownInBounds(const gfx::Rect& bounds) const;

  // Opens a sub-popup on a new cell (and closes the open one if any), or just
  // closes the existing if `absl::nullopt` is passed.
  void SetCellWithOpenSubPopup(absl::optional<CellIndex> cell_index,
                               PopupCellSelectionSource selection_source);

  // Controller for this view.
  base::WeakPtr<AutofillPopupController> controller_ = nullptr;

  // Parent's popup view. Present in sub-popups (non-root) only.
  absl::optional<base::WeakPtr<ExpandablePopupParentView>> parent_;

  // The index of the row with a selected cell.
  absl::optional<size_t> row_with_selected_cell_;

  // The latest cell which was set as having a sub-popup open. Storing it
  // is required to maintain the invariant of at most one such a cell.
  absl::optional<CellIndex> open_sub_popup_cell_;

  std::vector<RowPointer> rows_;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> body_container_ = nullptr;
  raw_ptr<views::BoxLayoutView> footer_container_ = nullptr;

  base::OneShotTimer open_sub_popup_timer_;
  base::OneShotTimer no_selection_sub_popup_close_timer_;

  base::WeakPtrFactory<PopupViewViews> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_H_
