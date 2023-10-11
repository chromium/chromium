// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_VIEW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/common/aliases.h"
#include "content/public/common/input/native_web_keyboard_event.h"
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
    // question `is_selected` or not, or `is_permanently_highlighted` or not.
    virtual void GetAccessibleNodeData(bool is_selected,
                                       bool is_permanently_highlighted,
                                       ui::AXNodeData* node_data) const = 0;
  };

  METADATA_HEADER(PopupCellView);

  PopupCellView();
  PopupCellView(const PopupCellView&) = delete;
  PopupCellView& operator=(const PopupCellView&) = delete;
  ~PopupCellView() override;

  // Gets and sets the selected state of the cell.
  bool GetSelected() const { return selected_; }
  virtual void SetSelected(bool selected);

  // Sets the highlighted state of the cell, for which there is an external
  // reason like opening a sub-popup.
  void SetPermanentlyHighlighted(bool permanently_highlighted);

  bool IsHighlighted() const;

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
  using OnAcceptedCallback = base::RepeatingCallback<void(base::TimeTicks)>;
  const PopupCellView::OnAcceptedCallback& GetOnAcceptedCallback() const {
    return on_accepted_callback_;
  }
  void SetOnAcceptedCallback(OnAcceptedCallback callback);
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
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;

 protected:
  // The selection state.
  bool selected_ = false;
  bool permanently_highlighted_ = false;
  base::RepeatingClosure on_selected_callback_;
  base::RepeatingClosure on_unselected_callback_;

 private:
  // Computes the actual `TimeTicks` at which the event occurred (taking latency
  // into account) and runs the OnAccepted callback.
  void RunOnAcceptedForEvent(const ui::Event& event);

  // views::View:
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // The tooltip text for this cell.
  std::u16string tooltip_text_;
  // The accessibility delegate.
  std::unique_ptr<AccessibilityDelegate> a11y_delegate_;

  base::RepeatingClosure on_entered_callback_;
  base::RepeatingClosure on_exited_callback_;
  OnAcceptedCallback on_accepted_callback_;

  // The labels whose style is updated when the cell's selection status changes.
  std::vector<raw_ptr<views::Label>> tracked_labels_;

};

BEGIN_VIEW_BUILDER(/* no export*/, PopupCellView, views::View)
VIEW_BUILDER_PROPERTY(std::u16string, TooltipText)
VIEW_BUILDER_PROPERTY(std::unique_ptr<PopupCellView::AccessibilityDelegate>,
                      AccessibilityDelegate)
VIEW_BUILDER_PROPERTY(PopupCellView::OnAcceptedCallback, OnAcceptedCallback)
VIEW_BUILDER_PROPERTY(base::RepeatingClosure, OnSelectedCallback)
VIEW_BUILDER_PROPERTY(base::RepeatingClosure, OnUnselectedCallback)
END_VIEW_BUILDER

}  // namespace autofill

DEFINE_VIEW_BUILDER(/*no export*/, autofill::PopupCellView)

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_CELL_VIEW_H_
