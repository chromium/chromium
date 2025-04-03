// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_TAB_COLLECTION_H_

#include <memory>

#include "chrome/browser/ui/tabs/tab_collection.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_controller.h"
#include "components/tab_groups/tab_group_visual_data.h"

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

namespace tabs {

class TabModel;

class TabGroupTabCollection : public TabCollection {
 public:
  TabGroupTabCollection(tab_groups::TabGroupId group_id,
                        tab_groups::TabGroupVisualData visual_data,
                        TabGroupController* controller);
  ~TabGroupTabCollection() override;
  TabGroupTabCollection(const TabGroupTabCollection&) = delete;
  TabGroupTabCollection& operator=(const TabGroupTabCollection&) = delete;

  // Returns the `group_id_` this collection is associated with.
  tab_groups::TabGroupId GetTabGroupId() const { return group_->id(); }

  // Returns the `group_` this collection is associated with.
  TabGroup* GetTabGroup() const { return group_.get(); }

  std::vector<tabs::TabModel*> GetTabs() const;

 private:
  // Group metadata for this collection.
  std::unique_ptr<TabGroup> group_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_TAB_COLLECTION_H_
