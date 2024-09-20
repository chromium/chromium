// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/autofill/next_idle_barrier.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "components/input/native_web_keyboard_event.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace content {
struct NativeWebKeyboardEvent;
}  // namespace content

namespace autofill {

class AutofillPopupController;
class PopupRowContentView;

// `PopupRowView` represents a single selectable popup row. It contains logic
// common to all row types (like selection callbacks or a11y) but it is not
// responsible for the view layout. It expects a `PopupRowContentView` instead.
// It also supports the expanding control depending on whether the suggestion
// has children or not (see `Suggestion::children`).
class PopupRowView : public views::View, public views::ViewObserver {
  METADATA_HEADER(PopupRowView, views::View)
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

    virtual std::optional<CellIndex> GetSelectedCell() const = 0;
    virtual void SetSelectedCell(std::optional<CellIndex> cell_index,
                                 PopupCellSelectionSource source) = 0;
  };

  // Returns the margin on the left and right of the row. When hovering in the
  // content cell, this is the distance one sees between the updated
  // background and the edge of the row.
  static int GetHorizontalMargin();

  PopupRowView(AccessibilitySelectionDelegate& a11y_selection_delegate,
               SelectionDelegate& selection_delegate,
               base::WeakPtr<AutofillPopupController> controller,
               int line_number,
               std::unique_ptr<PopupRowContentView> content_view);
  PopupRowView(const PopupRowView&) = delete;
  PopupRowView& operator=(const PopupRowView&) = delete;
  ~PopupRowView() override;

  // views::View:
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;

  // views::ViewObserver:
  void OnViewFocused(views::View* focused_now) override;

  // Gets and sets the selected cell within this row.
  std::optional<CellType> GetSelectedCell() const { return selected_cell_; }
  virtual void SetSelectedCell(std::optional<CellType> cell);

  // Sets whether the row's child suggestions are displayed in a sub-popup.
  // Note that the row doesn't control the sub-popup, but rather should be
  // synced with it by this method.
  void SetChildSuggestionsDisplayed(bool child_suggestions_displayed);

  // Returns the control cell's bounds. The cell must be present.
  gfx::RectF GetControlCellBounds() const;

  // Attempts to process a key press `event`. Returns true if it did (and the
  // parent no longer needs to handle it).
  virtual bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event);

  // Returns if the popup row is available for selection.
  virtual bool IsSelectable() const;

  // Returns the view representing the content area of the row.
  PopupRowContentView& GetContentView() { return *content_view_; }

  // Returns the view representing the suggestions expanding control of the row.
  views::View* GetExpandChildSuggestionsView() {
    return expand_child_suggestions_view_.get();
  }

  views::View* GetExpandChildSuggestionsIconViewForTesting() {
    return expand_child_suggestions_view_icon_.get();
  }

 protected:
  base::WeakPtr<AutofillPopupController> controller() { return controller_; }

  int line_number() const { return line_number_; }

 private:
  AccessibilitySelectionDelegate& GetA11ySelectionDelegate() {
    return a11y_selection_delegate_.get();
  }

  // Updates all UI parts that may have changed based on the current state,
  // for now they are the background and expand control visibility.
  void UpdateUI();

  // Updates the background according to the control cell highlighting state.
  void UpdateBackground();

  // Updates the expand subpopup icon visibility. By default the icon is
  // always visible in the case children suggestion exist. The exception is when
  // `CanUpdateOpenSubPopupIconVisibilityOnHover()` returns true. In this case
  // the icon is visible only when a cell is selected (e.g. when the row is
  // hovered) or the sub-popup is open.
  // TODO(crbug.com/40274514): Maybe remove this method once experiment is
  // complete.
  void UpdateOpenSubPopupIconVisibility();

  // This method is just a getter for the `barrier_for_accepting_` which is
  // set `true` when the view's visible part is big enough and was present on
  // the screen long enough, see `OnVisibleBoundsChanged()` implementation for
  // what these "enough"s mean.
  bool IsViewVisibleEnough() const;

  // The delegate used for accessibility announcements (implemented by the
  // parent view).
  const raw_ref<AccessibilitySelectionDelegate> a11y_selection_delegate_;
  // The delegate used for selection control (implemented by the parent view).
  const raw_ref<SelectionDelegate> selection_delegate_;
  // The controller for the parent view.
  const base::WeakPtr<AutofillPopupController> controller_;
  // The position of the row in the vertical list of suggestions.
  const int line_number_;

  // Which (if any) cell of this row is currently selected.
  std::optional<CellType> selected_cell_;

  // The view wrapping the content area of the row.
  raw_ptr<PopupRowContentView> content_view_ = nullptr;
  base::ScopedObservation<PopupRowContentView, views::ViewObserver>
      content_view_observer_{this};
  // The view wrapping the control area of the row.
  raw_ptr<views::View> expand_child_suggestions_view_ = nullptr;
  raw_ptr<views::View> expand_child_suggestions_view_icon_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      expand_child_suggestions_view_observer_{this};

  // Overriding event handles for the content and control views.
  std::unique_ptr<ui::EventHandler> content_event_handler_;
  std::unique_ptr<ui::EventHandler> control_event_handler_;

  // We want a mouse click to accept a suggestion only if the user has made an
  // explicit choice. Therefore, we shall ignore mouse clicks unless the mouse
  // has been moved into the item's screen bounds. For example, if the item is
  // hovered by the mouse at the time it's first shown, we want to ignore clicks
  // until the mouse has left and re-entered the bounds of the item
  // (crbug.com/1240472, crbug.com/1241585, crbug.com/1287364).
  // This is particularly relevant because mouse click interactions may be
  // processed with a delay, making it seem as if the two click interactions of
  // a double click were executed at intervals larger than the threshold (500ms)
  // checked in the controller (crbug.com/1418837).
  bool mouse_observed_outside_item_bounds_ = false;

  // Whether the `mouse_observed_outside_item_bounds_` will be ignored or not.
  // Today this happens when:
  // 1. The AutofillSuggestionTriggerSource is `kManualFallbackAddress`. This is
  // because in this situation even though the popup could appear behind the
  // cursor, the user intention about opening it is explicit.
  //
  // 2. The suggestions are of autocomplete type and were regenerated due to a
  // suggestion being removed. We want to ignore the check in this case because
  // the cursor can be above the popup after a row is deleted. This however does
  // not mean that the popup just showed up to the user so there is no need to
  // move the cursor out and in.
  const bool should_ignore_mouse_observed_outside_item_bounds_check_;

  // Whether the row's child suggestions (see `Suggestion::children`) are
  // displayed in a sub-popup.
  bool child_suggestions_displayed_ = false;

  // This is used to protected users from accepting suggestions too quickly.
  // This is often used in various attacks against their data when the user is
  // tricked to press a key combination or click a specific place on the screen
  // (e.g. in a game). Having a delay gives the user a chance to notice/overview
  // what they are about to expose to the website.
  // The timer starts in `OnVisibleBoundsChanged()` only when the view is
  // visible enough and checked before triggering acceptance on the controller.
  std::optional<NextIdleBarrier> barrier_for_accepting_;

  // Has the same value as `Suggestion::is_acceptable` of the underlying
  // suggestion. If `false` the content part is not highlighted separately,
  // but the whole row is highlighted instead as for the control view.
  const bool suggestion_is_acceptable_;

  const bool highlight_on_select_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_
