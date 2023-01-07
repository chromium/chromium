// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/tab_groups/tab_group_id.h"

class Profile;

// Serves to instantiate and own the SavedTabGroup infrastructure for the
// browser.
class SavedTabGroupKeyedService : public KeyedService,
                                  public SavedTabGroupController {
 public:
  explicit SavedTabGroupKeyedService(Profile* profile);
  SavedTabGroupKeyedService(const SavedTabGroupKeyedService&) = delete;
  SavedTabGroupKeyedService& operator=(const SavedTabGroupKeyedService& other) =
      delete;
  ~SavedTabGroupKeyedService() override;

  SavedTabGroupModelListener* listener() { return &listener_; }
  SavedTabGroupModel* model() { return &model_; }
  SavedTabGroupSyncBridge* bridge() { return &bridge_; }
  Profile* profile() { return profile_; }

  // SavedTabGroupController
  void OpenSavedTabGroupInBrowser(Browser* browser,
                                  const base::GUID& saved_group_guid) override;
  void SaveGroup(const tab_groups::TabGroupId& group_id,
                 Browser* browser = nullptr) override;
  void UnsaveGroup(const tab_groups::TabGroupId& group_id) override;
  void DisconnectLocalTabGroup(const tab_groups::TabGroupId& group_id) override;

 private:
  // Returns the ModelTypeStoreFactory tied to the current profile.
  syncer::OnceModelTypeStoreFactory GetStoreFactory();

  // The profile used to instantiate the keyed service.
  raw_ptr<Profile> profile_ = nullptr;

  // The current representation of this profiles saved tab groups.
  SavedTabGroupModel model_;

  // Listens to and observers all tabstrip models; updating the
  // SavedTabGroupModel when necessary.
  SavedTabGroupModelListener listener_;

  // Stores SavedTabGroup data to the disk and to sync if enabled.
  SavedTabGroupSyncBridge bridge_;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_
