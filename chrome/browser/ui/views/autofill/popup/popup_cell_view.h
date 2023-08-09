// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/aliases.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace content {
struct NativeWebKeyboardEvent;
}  // namespace content

namespace views {
class Label;
}  // namespace views

namespace autofill {

// `PopupCellView` represents a single, selectable cell. It is responsible
// for maintaining the "selected" state and updating it based on mouse event
// information.
class PopupCellView : public views::View {
 public:
  // Interface for injecting accessibility data into `PopupCellView`. This
  // allows to have `PopupCellViews` with different a11y roles without needing
  // to subclass them.
  class AccessibilityDelegate {
   public:
    virtual ~AccessibilityDelegate() = default;

    // Sets the a11y information in `node_data` based on whether the cell in
    // question `is_selected` or not.
    virtual void GetAccessibleNodeData(bool is_selected,
                                       ui::AXNodeData* node_data) const = 0;
  };

  METADATA_HEADER(PopupCellView);

  explicit PopupCellView(
      bool should_ignore_mouse_observed_outside_item_bounds_check = false);

  PopupCellView(const PopupCellView&) = delete;
  PopupCellView& operator=(const PopupCellView&) = delete;
  ~PopupCellView() override;

  // Gets and sets the selected state of the cell.
  bool GetSelected() const { return selected_; }
  virtual void SetSelected(bool selected);

  // Gets and sets the tooltip of the cell.
  const std::u16string& GetTooltipText() const { return tooltip_text_; }
  void SetTooltipText(std::u16string tooltip_text);

  // Sets the accessibility delegate that is consulted when providing accessible
  // node data.
  void SetAccessibilityDelegate(
      std::unique_ptr<AccessibilityDelegate> a11y_delegate);

  // Gets and sets the callback that is run when the cell is entered (via mouse
  // or gesture event).
  const base::RepeatingClosure& GetOnEnteredCallback() const {
    return on_entered_callback_;
  }
  void SetOnEnteredCallback(base::RepeatingClosure callback);
  // Gets and sets the callback that is run when the cell is exited.
  const base::RepeatingClosure& GetOnExitedCallback() const {
    return on_exited_callback_;
  }
  void SetOnExitedCallback(base::RepeatingClosure callback);
  // Gets and sets the callback that is run when the cell is accepted (left
  // mouse click, tap, enter key).
  const base::RepeatingClosure& GetOnAcceptedCallback() const {
    return on_accepted_callback_;
  }
  void SetOnAcceptedCallback(base::RepeatingClosure callback);
  // Gets and sets the callbacks for when the cell is (un)selected.
  const base::RepeatingClosure& GetOnSelectedCallback() const {
    return on_selected_callback_;
  }
  void SetOnSelectedCallback(base::RepeatingClosure callback);
  const base::RepeatingClosure& GetOnUnselectedCallback() const {
    return on_unselected_callback_;
  }
  void SetOnUnselectedCallback(base::RepeatingClosure callback);

  // Adds `label` to a list of labels whose style is refreshed whenever the
  // selection status of the cell changes. Assumes that `label` is a child of
  // `this` that will not be removed until `this` is destroyed.
  void TrackLabel(views::Label* label);

  // Updates the color of the view's background and adjusts the style of the
  // labels contained in it based on the selection status of the view.
  void RefreshStyle();

  // Attempts to process a key press `event`. Returns true if it did (and the
  // parent no longer needs to handle it).
  virtual bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event);

  // views::View:
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;

  void OnPaint(gfx::Canvas* canvas) override;

 protected:
  // The selection state.
  bool selected_ = false;
  base::RepeatingClosure on_selected_callback_;
  base::RepeatingClosure on_unselected_callback_;

 private:
  // Returns true if the mouse is within the bounds of this item. This is not
  // affected by whether or not the item is overlaid by another popup.
  bool IsMouseInsideItemBounds() const { return IsMouseHovered(); }

  // views::View:
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // The tooltip text for this cell.
  std::u16string tooltip_text_;
  // The accessibility delegate.
  std::unique_ptr<AccessibilityDelegate> a11y_delegate_;

  base::RepeatingClosure on_entered_callback_;
  base::RepeatingClosure on_exited_callback_;
  base::RepeatingClosure on_accepted_callback_;

  // The labels whose style is updated when the cell's selection status changes.
  std::vector<raw_ptr<views::Label>> tracked_labels_;

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
  // 1. The AutofillSuggestionTriggerSource is
  // `kManualFallbackForAutocompleteUnrecognized`. This is because in this
  // situation even though the popup could appear behind the cursor, the user
  // intention about opening it is explicit.
  //
  // 2. The suggestions are of autocomplete type and were regenerated due to a
  // suggestion being removed. We want to ignore the check in this case because
  // the cursor can be above the popup after a row is deleted. This however does
  // not mean that the popup just showed up to the user so there is no need to
  // move the cursor out and in.
  bool should_ignore_mouse_observed_outside_item_bounds_check_;
};

BEGIN_VIEW_BUILDER(/* no export*/, PopupCellView, views::View)
VIEW_BUILDER_PROPERTY(std::u16string, TooltipText)
VIEW_BUILDER_PROPERTY(std::unique_ptr<PopupCellView::AccessibilityDelegate>,
                      AccessibilityDelegate)
VIEW_BUILDER_PROPERTY(base::RepeatingClosure, OnEnteredCallback)
VIEW_BUILDER_PROPERTY(base::RepeatingClosure, OnExitedCallback)
VIEW_BUILDER_PROPERTY(base::RepeatingClosure, OnAcceptedCallback)
VIEW_BUILDER_PROPERTY(base::RepeatingClosure, OnSelectedCallback)
VIEW_BUILDER_PROPERTY(base::RepeatingClosure, OnUnselectedCallback)
END_VIEW_BUILDER

}  // namespace autofill

DEFINE_VIEW_BUILDER(/*no export*/, autofill::PopupCellView)

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_VIEW_H_
