// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/user_education/scoped_new_badge_tracker.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace content {
struct NativeWebKeyboardEvent;
}  // namespace content

class ScopedNewBadgeTracker;

namespace autofill {

class AutofillPopupController;
class PopupCellView;
class PopupRowStrategy;
class PopupViewViews;

// `PopupRowView` represents a single selectable popup row. Different styles
// of the row can be achieved by injecting the respective `PopupRowStrategy`
// objects in the constructor.
class PopupRowView : public views::View, public views::ViewObserver {
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
    virtual void SetSelectedCell(absl::optional<CellIndex> cell_index,
                                 PopupCellSelectionSource source) = 0;
  };

  // The tracker for a "new" badge that a row might have.
  class ScopedNewBadgeTrackerWithAcceptAction {
   public:
    ScopedNewBadgeTrackerWithAcceptAction(
        std::unique_ptr<ScopedNewBadgeTracker> tracker,
        const char* action_name);
    ~ScopedNewBadgeTrackerWithAcceptAction();

    ScopedNewBadgeTrackerWithAcceptAction(
        ScopedNewBadgeTrackerWithAcceptAction&&);
    ScopedNewBadgeTrackerWithAcceptAction& operator=(
        ScopedNewBadgeTrackerWithAcceptAction&&);

    // Notifies the tracker that the accept action was performed, i.e. the
    // feature was opened.
    void OnSuggestionAccepted();

   private:
    // The actual badge tracker.
    std::unique_ptr<ScopedNewBadgeTracker> tracker_;
    // The name of the action that is triggered on accepting the suggestion.
    const char* action_name_;
  };

  METADATA_HEADER(PopupRowView);
  PopupRowView(AccessibilitySelectionDelegate& a11y_selection_delegate,
               SelectionDelegate& selection_delegate,
               base::WeakPtr<AutofillPopupController> controller,
               int line_number,
               std::unique_ptr<PopupRowStrategy> strategy,
               std::optional<ScopedNewBadgeTrackerWithAcceptAction>
                   new_badge_tracker = std::nullopt);
  PopupRowView(const PopupRowView&) = delete;
  PopupRowView& operator=(const PopupRowView&) = delete;
  ~PopupRowView() override;

  // Acts as a factory method for creating a row view.
  static std::unique_ptr<PopupRowView> Create(PopupViewViews& popup_view,
                                              int line_number);

  // views::View:
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // views::ViewObserver:
  void OnViewFocused(views::View* focused_now) override;

  // Gets and sets the selected cell within this row.
  absl::optional<CellType> GetSelectedCell() const { return selected_cell_; }
  void SetSelectedCell(absl::optional<CellType> cell);

  // Sets whether the row's child suggestions are displayed in a sub-popup.
  // Note that the row doesn't control the sub-popup, but rather should be
  // synced with it by this method.
  void SetChildSuggestionsDisplayed(bool child_suggestions_displayed);

  // Returns the control cell's bounds. The cell must be present.
  gfx::RectF GetControlCellBounds() const;

  // Attempts to process a key press `event`. Returns true if it did (and the
  // parent no longer needs to handle it).
  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event);

  // Returns the view representing the content area of the row.
  PopupCellView& GetContentView() { return *content_view_; }
  // Returns the view representing the control area of the row. Can be null.
  PopupCellView* GetControlView() { return control_view_.get(); }

 private:
  void RunOnAcceptedForEvent(const ui::Event& event);

  // Returns the cell view or `nullptr` if it was not created.
  const PopupCellView* GetCellView(CellType type) const;
  PopupCellView* GetCellView(CellType type);

  AccessibilitySelectionDelegate& GetA11ySelectionDelegate() {
    return a11y_selection_delegate_.get();
  }

  // Updates the background according to the control cell highlighting state.
  void UpdateBackground();

  // The delegate used for accessibility announcements (implemented by the
  // parent view).
  const raw_ref<AccessibilitySelectionDelegate> a11y_selection_delegate_;
  // The delegate used for selection control (implemented by the parent view).
  const raw_ref<SelectionDelegate> selection_delegate_;
  // The controller for the parent view.
  const base::WeakPtr<AutofillPopupController> controller_;
  // The position of the row in the vertical list of suggestions.
  const int line_number_;
  // A tracker for "new" badges inside a cell. If set, it logs a performed
  // action on accepting the suggestion.
  std::optional<ScopedNewBadgeTrackerWithAcceptAction> new_badge_tracker_;
  // The strategy from which the actual view content of this row is created.
  const std::unique_ptr<PopupRowStrategy> strategy_;

  // Which (if any) cell of this row is currently selected.
  absl::optional<CellType> selected_cell_;

  // The cell wrapping the content area of the row.
  raw_ptr<PopupCellView> content_view_ = nullptr;
  // The cell wrapping the control area of the row.
  // TODO(crbug.com/1411172): Add keyboard event handling.
  raw_ptr<PopupCellView> control_view_ = nullptr;

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
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_VIEW_H_
