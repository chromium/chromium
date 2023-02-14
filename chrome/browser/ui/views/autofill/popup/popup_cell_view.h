// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace autofill {

// `PopupCellView` represents a single, selectable cell. It is responsible
// for maintaining the "selected" state and updating it based on mouse event
// information.
class PopupCellView : public views::View {
 public:
  METADATA_HEADER(PopupCellView);

  PopupCellView();
  PopupCellView(const PopupCellView&) = delete;
  PopupCellView& operator=(const PopupCellView&) = delete;
  ~PopupCellView() override;

  // Gets and sets the selected state of the cell.
  bool GetSelected() const { return selected_; }
  void SetSelected(bool selected);

  // Gets and sets the string announced by VoiceOver.
  const std::u16string& GetVoiceOverString() const { return voice_over_; }
  void SetVoiceOverString(std::u16string voice_over);

  // Gets and sets additional (optional) accessibility information. See the
  // member definition for more information.
  absl::optional<int> GetSetSizeForAccessibility() const { return set_size_; }
  void SetSetSizeForAccessibility(absl::optional<int> set_size);
  absl::optional<int> GetSetIndexForAccessibility() const { return set_index_; }
  void SetSetIndexForAccessibility(absl::optional<int> set_index);

  // Sets the callback that is run when the cell is entered (via mouse or
  // gesture event).
  void SetOnEnteredCallback(base::RepeatingClosure callback);
  // Sets the callback that is run when the cell is exited.
  void SetOnExitedCallback(base::RepeatingClosure callback);
  // Sets the callback that is run when the cell is accepted (left mouse click,
  // tap, enter key).
  void SetOnAcceptedCallback(base::RepeatingClosure callback);

  // Adds `label` to a list of labels whose style is refreshed whenever the
  // selection status of the cell changes. Assumes that `label` is a child of
  // `this` that will not be removed until `this` is destroyed.
  void TrackLabel(views::Label* label);

  // Updates the color of the view's background and adjusts the style of the
  // labels contained in it based on the selection status of the view.
  void RefreshStyle();

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

 private:
  // Returns true if the mouse is within the bounds of this item. This is not
  // affected by whether or not the item is overlaid by another popup.
  bool IsMouseInsideItemBounds() const { return IsMouseHovered(); }

  // The selection state.
  bool selected_ = false;
  // The string announced by VoiceOver.
  std::u16string voice_over_;

  // Additional information set for a11y purposes. The `set_size_` is the number
  // of non-separator suggestions and `set_index_` is this element's (1-indexed)
  // position in it.
  // TODO(crbug.com/1411172): Move to subclasses once `PopupStrategy` exists
  // and uses them since in the future not every `PopupCellView` may be a
  // `ListBoxOption`.
  absl::optional<int> set_index_;
  absl::optional<int> set_size_;

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
  bool mouse_observed_outside_item_bounds_ = false;
};

BEGIN_VIEW_BUILDER(/* no export*/, PopupCellView, views::View)
VIEW_BUILDER_PROPERTY(std::u16string, VoiceOverString)
VIEW_BUILDER_PROPERTY(absl::optional<int>, SetSizeForAccessibility)
VIEW_BUILDER_PROPERTY(absl::optional<int>, SetIndexForAccessibility)
END_VIEW_BUILDER

}  // namespace autofill

DEFINE_VIEW_BUILDER(/*no export*/, autofill::PopupCellView)

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_VIEW_H_
