// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SHARED_TAB_GROUP_FEEDBACK_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SHARED_TAB_GROUP_FEEDBACK_CONTROLLER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_id.h"

class BrowserWindowInterface;

namespace tab_groups {

// This class controls when the shared tab groups feedback button is displayed
// in the  pinned actions toolbar and when its IPH is triggered.
class SharedTabGroupFeedbackController : public TabStripModelObserver,
                                         public TabGroupSyncService::Observer {
 public:
  explicit SharedTabGroupFeedbackController(BrowserWindowInterface* browser);
  SharedTabGroupFeedbackController(const SharedTabGroupFeedbackController&) =
      delete;
  SharedTabGroupFeedbackController operator=(
      const SharedTabGroupFeedbackController& other) = delete;
  ~SharedTabGroupFeedbackController() override;

  // Only show the feedback button once the PinnedToolbarActionsContainer is
  // available.
  void Init();

  // Remove observers before `browser_` is destroyed.
  void TearDown();

 private:
  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // TabGroupSyncService::Observer:
  void OnInitialized() override;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;

  // Helper functions to ephemerally display the feedback button.
  void MaybeShowFeedbackActionInToolbar();
  void UpdateFeedbackButtonVisibility(bool should_show_button);

  // Only show the IPH when a shared tab becomes the active tab.
  void MaybeShowIPH(BrowserWindowInterface* browser_window_interface);

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  const raw_ptr<TabGroupSyncService> tab_group_sync_service_;

  std::vector<base::CallbackListSubscription> active_tab_change_subscriptions_;

  base::ScopedObservation<TabGroupSyncService, TabGroupSyncService::Observer>
      tab_group_sync_observer_{this};
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SHARED_TAB_GROUP_FEEDBACK_CONTROLLER_H_
