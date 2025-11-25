
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_ON_CLOSE_HELPER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_ON_CLOSE_HELPER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"

namespace tab_groups {
class TabGroupSyncService;

// Listens for notifications about when a tab is being deleted. When it is
// deleted, this class adds it to a specified closed saved tab group.
class SavedTabGroupOnCloseHelper : public content::WebContentsObserver {
 public:
  SavedTabGroupOnCloseHelper(TabGroupSyncService* service,
                             tabs::TabInterface* tab);

  ~SavedTabGroupOnCloseHelper() override;

  // Sets the closed saved group for which we will add |tab_| when
  // |tab_| is deleted.
  void SetGroup(const base::Uuid&);

  void UnsetGroup();

  // Testing function to check if we are set to add |tab_| to a closed
  // saved group when |tab_| is deleted.
  bool WillTryToAddToSavedGroupOnClose();

  // contents::WebContentsObserver:
  void BeforeUnloadDialogCancelled() override;

  // Callback to tabs::TabInterface::RegisterWillDetach to get updates
  // when |tab_| is closing.
  void OnTabClose(tabs::TabInterface* tab,
                  tabs::TabInterface::DetachReason reason);

 private:
  const raw_ptr<TabGroupSyncService> service_ = nullptr;
  const raw_ptr<tabs::TabInterface> tab_ = nullptr;
  std::optional<base::Uuid> saved_tab_group_id_ = std::nullopt;

  base::CallbackListSubscription tab_detach_subscription_;
};

}  // namespace tab_groups
#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_ON_CLOSE_HELPER_H_
