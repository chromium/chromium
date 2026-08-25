// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace actions {
class ActionItem;
}

namespace views {
class BubbleDialogModelHost;
}

class BrowserWindowInterface;

namespace memory_saver {

// This class is a helper used to update ActionItem's bubble state as
// MemorySaver's bubble shows and hides. When this feature is fully migrated,
// it may be preferable to merge this functionality into
// MemorySaverBubleDelegate.
class MemorySaverBubbleController : MemorySaverBubbleObserver {
 public:
  DECLARE_USER_DATA(MemorySaverBubbleController);

  explicit MemorySaverBubbleController(BrowserWindowInterface* bwi);
  ~MemorySaverBubbleController() override;

  // Returns the controller for `bwi`'s window, or null if it does not have
  // one.
  static MemorySaverBubbleController* From(BrowserWindowInterface* bwi);

  // Called by the ActionItem framework when the action is invoked, to show
  // the Memory Saver bubble.
  void InvokeAction(BrowserWindowInterface* bwi, actions::ActionItem* item);

  // MemorySaverBubbleObserver:
  void OnBubbleShown() override;
  void OnBubbleHidden() override;

  views::BubbleDialogModelHost* bubble_for_testing() { return bubble_; }

 private:
  raw_ptr<actions::ActionItem> action_item_;
  raw_ptr<views::BubbleDialogModelHost> bubble_ = nullptr;

  ui::ScopedUnownedUserData<MemorySaverBubbleController>
      scoped_unowned_user_data_;
};

}  // namespace memory_saver

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_CONTROLLER_H_
