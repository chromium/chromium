// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/memory_saver_bubble_controller.h"

#include <memory>

#include "base/check_op.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_bubble_view.h"
#include "ui/views/view.h"

namespace memory_saver {

MemorySaverBubbleController::MemorySaverBubbleController(
    BrowserWindowInterface* bwi) {
  // Associate the bubble with its ActionItem, to ensure that any future
  // invocations come from the expected ActionItem.
  actions::ActionItem* action = actions::ActionManager::Get().FindAction(
      kActionShowMemorySaverChip,
      /*scope=*/bwi->GetActions()->root_action_item());
  CHECK(action);
  action_item_ = action->GetAsWeakPtr();
}

MemorySaverBubbleController::~MemorySaverBubbleController() = default;

void MemorySaverBubbleController::InvokeAction(Browser* browser,
                                               actions::ActionItem* item) {
  CHECK(item == action_item_.get());

  // Open the dialog bubble.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  CHECK_NE(browser_view, nullptr);
  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAnchorView(std::nullopt);
  bubble_ = MemorySaverBubbleView::ShowBubble(browser, anchor_view, this);
}

void MemorySaverBubbleController::OnBubbleShown() {
  if (action_item_) {
    action_item_->SetIsShowingBubble(true);
  }
}

void MemorySaverBubbleController::OnBubbleHidden() {
  if (action_item_) {
    action_item_->SetIsShowingBubble(false);
  }
  bubble_ = nullptr;
}

}  // namespace memory_saver
