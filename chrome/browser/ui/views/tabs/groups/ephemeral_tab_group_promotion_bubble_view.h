// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_EPHEMERAL_TAB_GROUP_PROMOTION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_EPHEMERAL_TAB_GROUP_PROMOTION_BUBBLE_VIEW_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_bubble_view.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view_tracker.h"

class BrowserWindowInterface;
class ColorPickerView;
class TabGroupEditorTitleField;

namespace views {
class Textfield;
class View;
class Widget;
}  // namespace views

// The bubble shown for an ephemeral (non-group focus) tab group. Ephemeral
// groups only exist to back focus mode, so instead of the full tab group
// editor this bubble offers just a title, a color and a Create / Cancel pair:
// Create promotes the group into a regular tab group with the chosen visuals,
// Cancel discards the edits and leaves the group ephemeral.
//
// Regular groups are edited with TabGroupEditorBubbleView, which dispatches
// here from TabGroupEditorBubbleView::Show() when the group is ephemeral.
//
// This is the bubble's delegate; the contents are a plain views::View built in
// BuildContentsView(). The delegate is owned by the Widget returned from
// Show() (attached as an owned widget property), so callers only need to keep
// the Widget alive, exactly as for TabGroupEditorBubbleView.
class EphemeralTabGroupPromotionBubbleView : public views::BubbleDialogDelegate,
                                             public TabStripModelObserver,
                                             public views::TextfieldController,
                                             public display::DisplayObserver {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kEphemeralTabGroupPromotionBubbleViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCreateButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonId);

  // Shows the bubble for the ephemeral `group`. Returns the bubble's widget.
  static std::unique_ptr<views::Widget> Show(
      BrowserWindowInterface* browser,
      const tab_groups::TabGroupId& group,
      views::View* anchor_view,
      std::optional<gfx::Rect> anchor_rect,
      bool stop_context_menu_propagation);

  EphemeralTabGroupPromotionBubbleView(
      const EphemeralTabGroupPromotionBubbleView&) = delete;
  EphemeralTabGroupPromotionBubbleView& operator=(
      const EphemeralTabGroupPromotionBubbleView&) = delete;
  ~EphemeralTabGroupPromotionBubbleView() override;

  // views::BubbleDialogDelegate:
  gfx::Rect GetAnchorRect() const override;

  // TabStripModelObserver:
  void OnTabGroupChanged(const TabGroupChange& change) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

 private:
  EphemeralTabGroupPromotionBubbleView(BrowserWindowInterface* browser,
                                       const tab_groups::TabGroupId& group,
                                       views::View* anchor_view,
                                       std::optional<gfx::Rect> anchor_rect);

  // Builds heading + title field + color picker + Cancel/Create row and
  // registers the title field and color picker with their ViewTrackers.
  std::unique_ptr<views::View> BuildContentsView(
      bool stop_context_menu_propagation);
  std::unique_ptr<views::View> BuildButtonRow();

  TabGroupEditorTitleField* title_field();
  ColorPickerView* color_selector();

  // Applies the edited title and color to the group, promotes it to a regular
  // tab group and closes the bubble.
  void OnCreatePressed();
  // Closes the bubble without touching the group.
  void OnCancelPressed();

  void OnBrowserDidClose(BrowserWindowInterface* browser);

  raw_ptr<BrowserWindowInterface> browser_;
  const tab_groups::TabGroupId group_;

  // If true will use the `anchor_rect_` provided in the constructor, otherwise
  // fall back to using the anchor view bounds.
  const bool use_set_anchor_rect_;

  const TabGroupEditorBubbleView::Colors colors_;
  // Tracked via ViewTracker because Widget::~Widget() destroys the root view
  // hierarchy before ~PropertyHandler() destroys this delegate.
  views::ViewTracker title_field_tracker_;
  views::ViewTracker color_selector_tracker_;

  base::CallbackListSubscription browser_close_subscription_;

  // Scoped to the bubble so the observer only exists while the bubble is open.
  display::ScopedDisplayObserver display_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_EPHEMERAL_TAB_GROUP_PROMOTION_BUBBLE_VIEW_H_
