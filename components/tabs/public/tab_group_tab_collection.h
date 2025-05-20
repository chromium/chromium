// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_GROUP_TAB_COLLECTION_H_
#define COMPONENTS_TABS_PUBLIC_TAB_GROUP_TAB_COLLECTION_H_

#include <memory>

#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_collection.h"

class TabGroup;

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

namespace tabs {

class TabGroupTabCollection : public TabCollection {
 public:
  TabGroupTabCollection(tab_groups::TabGroupId group_id,
                        tab_groups::TabGroupVisualData visual_data);
  ~TabGroupTabCollection() override;
  TabGroupTabCollection(const TabGroupTabCollection&) = delete;
  TabGroupTabCollection& operator=(const TabGroupTabCollection&) = delete;

  // Returns the `group_` this collection is associated with.
  const TabGroup* GetTabGroup() const { return group_.get(); }
  TabGroup* GetTabGroup() { return group_.get(); }

  const tab_groups::TabGroupId& GetTabGroupId() const;

 private:
  // Group metadata for this collection.
  std::unique_ptr<TabGroup> group_;
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_GROUP_TAB_COLLECTION_H_
