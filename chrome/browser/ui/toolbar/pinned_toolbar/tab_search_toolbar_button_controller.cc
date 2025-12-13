// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/pinned_toolbar/tab_search_toolbar_button_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"

TabSearchToolbarButtonController::TabSearchToolbarButtonController(
    BrowserView* browser_view)
    : browser_view_(browser_view) {}

TabSearchToolbarButtonController::~TabSearchToolbarButtonController() = default;

void TabSearchToolbarButtonController::OnBubbleInitializing() {
  actions::ActionItem* tab_search_action_item = GetTabSearchActionItem();
  tab_search_action_item->SetIsShowingBubble(true);
  PinnedToolbarActionsContainer* pinned_toolbar_actions_container =
      browser_view_->toolbar()->pinned_toolbar_actions_container();

  if (pinned_toolbar_actions_container->IsActionPinned(kActionTabSearch)) {
    return;
  }

  pinned_toolbar_actions_container->ShowActionEphemerallyInToolbar(
      kActionTabSearch, true);
}

void TabSearchToolbarButtonController::OnBubbleDestroying() {
  actions::ActionItem* tab_search_action_item = GetTabSearchActionItem();
  tab_search_action_item->SetIsShowingBubble(false);
  PinnedToolbarActionsContainer* pinned_toolbar_actions_container =
      browser_view_->toolbar()->pinned_toolbar_actions_container();

  if (pinned_toolbar_actions_container->IsActionPinned(kActionTabSearch)) {
    return;
  }

  // Post a delayed task to give a chance for the user to use the context menu
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabSearchToolbarButtonController::
                         MaybeHideActionEphemerallyInToolbar,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(1));
}

void TabSearchToolbarButtonController::UpdateBubbleHost(
    TabSearchBubbleHost* new_tab_search_bubble_host) {
  tab_search_bubble_host_observation_.Reset();

  actions::ActionItem* action_item = GetTabSearchActionItem();
  CHECK(action_item);

  if (new_tab_search_bubble_host) {
    action_item->SetProperty(
        actions::kActionItemPinnableKey,
        static_cast<int>(actions::ActionPinnableState::kPinnable));
    tab_search_bubble_host_observation_.Observe(new_tab_search_bubble_host);
  } else {
    action_item->SetProperty(
        actions::kActionItemPinnableKey,
        static_cast<int>(actions::ActionPinnableState::kNotPinnable));
  }
}

void TabSearchToolbarButtonController::MaybeHideActionEphemerallyInToolbar() {
  PinnedToolbarActionsContainer* pinned_toolbar_actions_container =
      browser_view_->toolbar()->pinned_toolbar_actions_container();

  if (GetTabSearchActionItem()->GetIsShowingBubble() ||
      pinned_toolbar_actions_container->IsActionPinned(kActionTabSearch)) {
    return;
  }

  pinned_toolbar_actions_container->ShowActionEphemerallyInToolbar(
      kActionTabSearch, false);
}

actions::ActionItem*
TabSearchToolbarButtonController::GetTabSearchActionItem() {
  return actions::ActionManager::Get().FindAction(
      kActionTabSearch,
      browser_view_->browser()->browser_actions()->root_action_item());
}
