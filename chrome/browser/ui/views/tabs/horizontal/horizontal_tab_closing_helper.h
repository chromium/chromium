// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_HORIZONTAL_TAB_CLOSING_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_HORIZONTAL_TAB_CLOSING_HELPER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "components/tabs/public/tab_collection.h"
#include "ui/views/mouse_watcher.h"

class RootTabCollectionNode;
class TabCollectionNode;
class UnpinnedTabContainerView;

// Manages tab closing mode for the horizontal tab strip.
// Keeps tab sizes locked while closing tabs with the mouse to keep tab close
// buttons under the mouse cursor.
class HorizontalTabClosingHelper : public views::MouseWatcherListener {
 public:
  explicit HorizontalTabClosingHelper(RootTabCollectionNode& root_node);
  HorizontalTabClosingHelper(const HorizontalTabClosingHelper&) = delete;
  HorizontalTabClosingHelper& operator=(const HorizontalTabClosingHelper&) =
      delete;
  ~HorizontalTabClosingHelper() override;

  // Enters tab closing mode if tabs are below standard width and `source` is a
  // mouse/touch interaction. If `override_width` is provided, the container is
  // constrained to that width (e.g. when collapsing a tab group). Otherwise,
  // the width is calculated as tabs are closed.
  void MaybeEnterTabClosingMode(std::optional<int> override_width,
                                CloseTabSource source);
  void ExitTabClosingMode();
  void SetOverrideAvailableWidth(int override_width);

  // Returns the constrained available width if in tab closing mode.
  std::optional<int> override_available_width_for_tabs() const {
    return override_available_width_for_tabs_;
  }

  int GetUnpinnedContainerWidth() const;

  bool in_tab_close() const { return in_tab_close_; }

  void PauseMouseWatcher();
  void ResumeMouseWatcher();

  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override;

 private:
  void StartMouseWatcher();
  void StopMouseWatcher();
  void OnTouchTimerFired();

  void OnChildWillBeRemoved(TabCollectionNode* child_node);
  void OnChildrenAdded(const tabs::TabCollectionNodes& handles);
  void OnChildMoved(TabCollectionNode* moved_node);

  void InvalidateLayout();
  UnpinnedTabContainerView* GetUnpinnedContainer() const;
  int GetUnpinnedContainerTotalPreferredWidth() const;

  const raw_ref<RootTabCollectionNode> root_node_;

  bool in_tab_close_ = false;
  bool is_paused_ = false;
  std::optional<int> override_available_width_for_tabs_;
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;
  base::OneShotTimer touch_resize_timer_;

  base::CallbackListSubscription on_child_will_be_removed_subscription_;
  base::CallbackListSubscription on_children_added_subscription_;
  base::CallbackListSubscription on_child_moved_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_HORIZONTAL_TAB_CLOSING_HELPER_H_
