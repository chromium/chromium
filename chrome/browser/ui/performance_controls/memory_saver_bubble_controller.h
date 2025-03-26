// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_observer.h"

namespace actions {
class ActionItem;
}

namespace views {
class BubbleDialogModelHost;
}

class Browser;
class BrowserWindowInterface;

namespace memory_saver {

// This class is a helper used to update ActionItem's bubble state as
// MemorySaver's bubble shows and hides. When this feature is fully migrated,
// it may be preferable to merge this functionality into
// MemorySaverBubleDelegate.
class MemorySaverBubbleController : MemorySaverBubbleObserver {
 public:
  explicit MemorySaverBubbleController(BrowserWindowInterface* bwi);
  ~MemorySaverBubbleController() override;

  // Called by the ActionItem framework when the action is invoked, to show
  // the Memory Saver bubble.
  void InvokeAction(Browser* browser, actions::ActionItem* item);

  // MemorySaverBubbleObserver:
  void OnBubbleShown() override;
  void OnBubbleHidden() override;

  views::BubbleDialogModelHost* bubble_for_testing() { return bubble_; }

 private:
  base::WeakPtr<actions::ActionItem> action_item_;
  raw_ptr<views::BubbleDialogModelHost> bubble_ = nullptr;
};

}  // namespace memory_saver

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_CONTROLLER_H_
