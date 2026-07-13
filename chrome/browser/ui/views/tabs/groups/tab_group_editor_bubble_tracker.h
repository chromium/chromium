// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_TAB_GROUP_EDITOR_BUBBLE_TRACKER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_TAB_GROUP_EDITOR_BUBBLE_TRACKER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "ui/views/widget/widget.h"

namespace tabs {
class VerticalTabStripStateController;
}

namespace views {
class ScrollView;
}  // namespace views

// Tracks whether the editor bubble is open. At most one can be open
// at once.
class TabGroupEditorBubbleTracker {
 public:
  explicit TabGroupEditorBubbleTracker(
      tabs::VerticalTabStripStateController* state_controller);
  ~TabGroupEditorBubbleTracker();

  void SetScrollView(views::ScrollView* scroll_view);
  void Opened(std::unique_ptr<views::Widget> bubble_widget);
  bool is_open() const { return is_open_; }
  views::Widget* widget() const { return widget_.get(); }

  base::CallbackListSubscription RegisterOnBubbleOpened(
      base::RepeatingCallback<void()> callback);
  base::CallbackListSubscription RegisterOnBubbleClosed(
      base::RepeatingCallback<void()> callback);

 private:
  void OnVerticalTabStripModeWillChange(
      tabs::VerticalTabStripStateController* controller);
  void OnContentsScrolled();

  void CloseWidget(views::Widget::ClosedReason reason);
  void StopIgnoringScroll();

  bool is_open_ = false;
  bool ignore_scroll_ = false;
  std::unique_ptr<views::Widget> widget_;
  base::RepeatingCallbackList<void()> on_bubble_opened_callback_list_;
  base::RepeatingCallbackList<void()> on_bubble_closed_callback_list_;
  base::CallbackListSubscription vertical_tab_mode_will_change_subscription_;
  base::CallbackListSubscription scroll_view_subscription_;

  base::WeakPtrFactory<TabGroupEditorBubbleTracker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_TAB_GROUP_EDITOR_BUBBLE_TRACKER_H_
