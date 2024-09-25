// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/views/autofill/popup/password_favicon_loader.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_search_bar_view.h"
#include "components/autofill/core/common/aliases.h"
#include "components/input/native_web_keyboard_event.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace views {
class BoxLayoutView;
class ScrollView;
}  // namespace views

namespace autofill_prediction_improvements {
class PredictionImprovementsLoadingStateView;
}

namespace autofill {

class AutofillPopupController;
class AutofillSuggestionController;
class PopupSeparatorView;
class PopupTitleView;
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
                       public ExpandablePopupParentView,
                       public PopupSearchBarView::Delegate {
  METADATA_HEADER(PopupViewViews, PopupBaseView)

 public:
  using RowPointer = absl::variant<PopupRowView*,
                                   PopupSeparatorView*,
                                   PopupTitleView*,
                                   PopupWarningView*,
                                   autofill_prediction_improvements::
                                       PredictionImprovementsLoadingStateView*>;

  // The time it takes for a selected cell to open a sub-popup if it has one.
  static constexpr base::TimeDelta kMouseOpenSubPopupDelay =
      base::Milliseconds(250);
  static constexpr base::TimeDelta kNonMouseOpenSubPopupDelay =
      kMouseOpenSubPopupDelay / 10;

  // The delay for closing the sub-popup after having no cell selected,
  // sub-popup cells are also taken into account.
  static constexpr base::TimeDelta kNoSelectionHideSubPopupDelay =
      base::Milliseconds(2500);

  // Constructor for creating sub-popups.
  PopupViewViews(base::WeakPtr<AutofillPopupController> controller,
                 base::WeakPtr<ExpandablePopupParentView> parent,
                 views::Widget* parent_widget);

  // Constructor for creating root level popups. Providing `std::nullopt` to
  // the `search_bar_config` results in creating a popup without a search bar.
  explicit PopupViewViews(
      base::WeakPtr<AutofillPopupController> controller,
      std::optional<const AutofillPopupView::SearchBarConfig>
          search_bar_config = std::nullopt);
  PopupViewViews(const PopupViewViews&) = delete;
  PopupViewViews& operator=(const PopupViewViews&) = delete;
  ~PopupViewViews() override;

  base::WeakPtr<AutofillPopupController> controller() { return controller_; }
  // Gets and sets the currently selected cell. If an invalid `cell_index` is
  // passed, `GetSelectedCell()` will return `std::nullopt` afterwards.
  std::optional<CellIndex> GetSelectedCell() const override;
  void SetSelectedCell(std::optional<CellIndex> cell_index,
                       PopupCellSelectionSource source) override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // AutofillPopupView:
  bool Show(AutoselectFirstSuggestion autoselect_first_suggestion) override;
  void Hide() override;
  bool OverlapsWithPictureInPictureWindow() const override;
  std::optional<int32_t> GetAxUniqueId() override;
  void AxAnnounce(const std::u16string& text) override;
  base::WeakPtr<AutofillPopupView> CreateSubPopupView(
      base::WeakPtr<AutofillSuggestionController> controller) override;
  std::optional<AutofillClient::PopupScreenLocation> GetPopupScreenLocation()
      const override;
  bool HasFocus() const override;
  base::WeakPtr<AutofillPopupView> GetWeakPtr() override;

  // PopupBaseView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // PopupSearchBarView::Delegate:
  void SearchBarOnInputChanged(const std::u16string& text) override;
  void SearchBarOnFocusLost() override;
  bool SearchBarHandleKeyPressed(const ui::KeyEvent& event) override;

 private:
  friend class PopupViewViewsTestApi;

  // Sets the `cell_index` cell as selected (or just unselects the selected
  // cell if `nullopt` is passed). Only one cell can be selected at a time.
  // Depending on the cell type and the suggestion, it makes the cell or
  // the whole row highlighted. It may also trigger the form preview or
  // a sub-popup to open, which depends on the suggestion acceptability, whether
  // it has children children and the cell type. `autoselect_first_suggestion`
  // and `suppress_popup` are relevant for cases when setting the cell triggers
  // a sub-popup to open. Setting `suppress_popup` to `true` prevents
  // the sub-popup from opening in such cases. `autoselect_first_suggestion`
  // controls whether the first suggestion in the sub-popup will be selected,
  // also capturing the keyboard focus if it is `true`.
  void SetSelectedCell(std::optional<CellIndex> cell_index,
                       PopupCellSelectionSource source,
                       AutoselectFirstSuggestion autoselect_first_suggestion,
                       bool suppress_popup = false);

  // Returns the `PopupRowView` at line number `index`. Assumes that there is
  // such a view at that line number - otherwise the underlying variant will
  // check false.
  PopupRowView& GetPopupRowViewAt(size_t index) {
    return *absl::get<PopupRowView*>(rows_[index]);
  }
  const PopupRowView& GetPopupRowViewAt(size_t index) const {
    return *absl::get<PopupRowView*>(rows_[index]);
  }

  void UpdateExpandedCollapsedAccessibleState() const;

  // Returns whether the row at `index` exists, is a `PopupRowView` and is
  // selectable.
  bool HasSelectablePopupRowViewAt(size_t index) const;

  // Instantiates the content of the popup.
  void InitViews();

  // Creates child views based on the suggestions given by |controller_|.
  // This method expects that all non-footer suggestions precede footer
  // suggestions.
  void CreateSuggestionViews();

  // Selects the first row prior to the currently selected one that is
  // selectable (e.g. not a separator). If no row is selected or no row prior to
  // the current one is selectable, it tries to select the last row. If that one
  // is unselectable, no row is selected.
  void SelectPreviousRow();

  // Analogous to previous row, just in the opposite direction: Tries to find
  // the next selectable row after the currently selected one and selects it
  // with the given selection source. If no row is selected or no row following
  // the currently selected one is selectable, it tries to select the first
  // row. If that one is unselectable, no row is selected.
  void SelectNextRow(PopupCellSelectionSource source);

  // Selects the next/previous in horizontal direction (i.e. left to right or
  // vice versa) cell, if there is one. Otherwise leaves the current selection.
  // Does not wrap.
  bool SelectNextHorizontalCell();
  bool SelectPreviousHorizontalCell();

  // Attempts to accept the selected cell. It will return false if there is no
  // selected cell or the cell does not trigger field filling or scanning a
  // credit card.
  bool AcceptSelectedContentOrCreditCardCell();

  // Attempts to remove the selected cell. Only content cells are allowed to be
  // selected.
  bool RemoveSelectedCell();

  // Reacts to key events under the assumption that the currently shown popup
  // contains Compose content.
  bool HandleKeyPressEventForCompose(
      const input::NativeWebKeyboardEvent& event);

  // AutofillPopupView:
  bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event) override;
  void OnSuggestionsChanged(bool prefer_prev_arrow_side) override;

  // PopupBaseView:
  bool DoUpdateBoundsAndRedrawPopup() override;

  // If `prefer_prev_arrow_side` is `true`, the view takes prev arrow side as
  // the first preferred when recalculating the popup position.
  bool DoUpdateBoundsAndRedrawPopup(bool prefer_prev_arrow_side);

  // ExpandablePopupParentView:
  void OnMouseEnteredInChildren() override;
  void OnMouseExitedInChildren() override;

  // Returns whether the footer container is scrollable with other suggestions
  // or it is "sticky" (i.e. it has a fixed position, always visible and
  // the non-footer suggestions are scrolled independently).
  bool IsFooterScrollable() const;

  bool CanShowDropdownInBounds(const gfx::Rect& bounds) const;

  // Opens a sub-popup on a new row (and closes the open one if any), or just
  // closes the existing if `std::nullopt` is passed.
  void SetRowWithOpenSubPopup(
      std::optional<size_t> row_index,
      AutoselectFirstSuggestion autoselect_first_suggestion =
          AutoselectFirstSuggestion(false));

  // Attempts to select the content cell of the row with the currently open
  // sub-popup. This closes the sub-popup and has the effect of going one menu
  // level up. Returns whether this was successful.
  bool SelectParentPopupContentCell();

  // Announces a string without assertively alerting a user.
  void AnnouncePolitely(const std::u16string& text);

  // Controller for this view.
  base::WeakPtr<AutofillPopupController> controller_ = nullptr;

  // Parent's popup view. Present in sub-popups (non-root) only.
  std::optional<base::WeakPtr<ExpandablePopupParentView>> parent_;

  std::unique_ptr<PasswordFaviconLoaderImpl> password_favicon_loader_;

  // The index of the row with a selected cell.
  std::optional<size_t> row_with_selected_cell_;

  // The latest row which was set as having a sub-popup open. Storing it
  // is required to maintain the invariant of at most one such a row.
  std::optional<size_t> row_with_open_sub_popup_;

  std::vector<RowPointer> rows_;
  const std::optional<const AutofillPopupView::SearchBarConfig>
      search_bar_config_;
  raw_ptr<PopupSearchBarView> search_bar_ = nullptr;
  raw_ptr<views::BoxLayoutView> suggestions_container_ = nullptr;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> body_container_ = nullptr;
  raw_ptr<views::BoxLayoutView> footer_container_ = nullptr;

  base::OneShotTimer open_sub_popup_timer_;
  base::OneShotTimer no_selection_sub_popup_close_timer_;

  // Defines whether the popup handles keyboard events like UP/DOWN/ESC/etc.
  // This value is important for defining which popup handles the event when
  // a chain of (sub-)popups is open: having no focus for a sub-popup means
  // that its parent will take care of handling it.
  // It's automatically set `true` for the root popup (so that it always handles
  // events) and when something is selected in sub-popups. The initial value is
  // set in `Show()`, but after that once it is `true` the value never gets back
  // to `false.`
  bool has_keyboard_focus_ = false;

  // A boolean variable tracking the execution state of the `Show()` method.
  // It's set to `true` when the method starts and `false` before it finishes.
  // Required for the `HasFocus()` method, that might be called during
  // the `Show()` execution. In this case, `GetWidget()->IsActive()` may not yet
  // be `true`, and the `HasFocus()` returning `false` may potentially cause
  // the popup to hide.
  bool show_in_progress_ = false;

  base::WeakPtrFactory<PopupViewViews> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_VIEW_VIEWS_H_
