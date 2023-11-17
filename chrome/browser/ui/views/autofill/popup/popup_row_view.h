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
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/user_education/scoped_new_badge_tracker.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace content {
struct NativeWebKeyboardEvent;
}  // namespace content

class ScopedNewBadgeTracker;

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

  PopupRowView(AccessibilitySelectionDelegate& a11y_selection_delegate,
               SelectionDelegate& selection_delegate,
               base::WeakPtr<AutofillPopupController> controller,
               int line_number,
               std::unique_ptr<PopupRowContentView> content_view);
  PopupRowView(const PopupRowView&) = delete;
  PopupRowView& operator=(const PopupRowView&) = delete;
  ~PopupRowView() override;

  void set_new_badge_tracker(
      std::optional<ScopedNewBadgeTrackerWithAcceptAction> new_badge_tracker) {
    new_badge_tracker_ = std::move(new_badge_tracker);
  }

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
  virtual void SetSelectedCell(absl::optional<CellType> cell);

  // Sets whether the row's child suggestions are displayed in a sub-popup.
  // Note that the row doesn't control the sub-popup, but rather should be
  // synced with it by this method.
  void SetChildSuggestionsDisplayed(bool child_suggestions_displayed);

  // Returns the control cell's bounds. The cell must be present.
  gfx::RectF GetControlCellBounds() const;

  // Attempts to process a key press `event`. Returns true if it did (and the
  // parent no longer needs to handle it).
  virtual bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event);

  // Returns the view representing the content area of the row.
  PopupRowContentView& GetContentView() { return *content_view_; }

  // Returns the view representing the suggestions expanding control of the row.
  views::View* GetExpandChildSuggestionsView() {
    return expand_child_suggestions_view_.get();
  }

 protected:
  base::WeakPtr<AutofillPopupController> controller() { return controller_; }

  int line_number() const { return line_number_; }

 private:
  // If the suggestion has child suggestions the row view adds this view to
  // provide a control for the sub-popup. It implements visualization and event
  // handling only, `PopupViewViews` controls the logic of opening/closing.
  class ExpandChildSuggestionsView : public views::View {
   public:
    METADATA_HEADER(ExpandChildSuggestionsView);
    ExpandChildSuggestionsView();
    ExpandChildSuggestionsView(const ExpandChildSuggestionsView&) = delete;
    ExpandChildSuggestionsView& operator=(const ExpandChildSuggestionsView&) =
        delete;
    ~ExpandChildSuggestionsView() override = default;

    // views::View:
    void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

    // Sets the a11y checked state. It should reflect the sub-popup open state.
    void SetChecked(bool checked);

   private:
    // This property controls the a11y `ax::mojom::CheckedState` attribute.
    // The value is controlled by external clients (see `SetChecked()`) and
    // expected to be synced with the sub-popup open state.
    bool checked_ = false;
  };

  void RunOnAcceptedForEvent(const ui::Event& event);

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

  // Which (if any) cell of this row is currently selected.
  absl::optional<CellType> selected_cell_;

  // The cell wrapping the content area of the row.
  raw_ptr<PopupRowContentView> content_view_ = nullptr;
  // The cell wrapping the control area of the row.
  // TODO(crbug.com/1411172): Add keyboard event handling.
  raw_ptr<ExpandChildSuggestionsView> expand_child_suggestions_view_ = nullptr;

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
