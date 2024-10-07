// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_TAB_COLLECTION_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/tab_collection.h"
#include "components/tab_groups/tab_group_id.h"

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

namespace tabs {

class TabModel;
class TabCollectionStorage;

class TabGroupTabCollection : public TabCollection {
 public:
  explicit TabGroupTabCollection(tab_groups::TabGroupId group_id);
  ~TabGroupTabCollection() override;
  TabGroupTabCollection(const TabGroupTabCollection&) = delete;
  TabGroupTabCollection& operator=(const TabGroupTabCollection&) = delete;

  // Adds a `tab_model` to the group at a particular index.
  void AddTab(std::unique_ptr<TabModel> tab_model, size_t index);

  // Appends a `tab_model` to the end of the group.
  void AppendTab(std::unique_ptr<TabModel> tab_model);

  // Moves a `tab_model` to the `dst_index` within the group.
  void MoveTab(TabModel* tab_model, size_t dst_index);

  // Removes and cleans the `tab_model`.
  void CloseTab(TabModel* tab_model);

  // Returns the `group_id_` this collection is associated with.
  tab_groups::TabGroupId GetTabGroupId() const { return group_id_; }

  // Returns the tab at a direct child index in this collection. If the index is
  // invalid it returns nullptr.
  tabs::TabModel* GetTabAtIndex(size_t index) const;

  // TabCollection:
  bool ContainsTab(TabModel* tab_model) const override;
  // This is non-recursive for grouped tab collection as it does not contain
  // another collection.
  bool ContainsTabRecursive(TabModel* tab_model) const override;

  // This is false as grouped tab collection does not contain another
  // collection.
  bool ContainsCollection(TabCollection* collection) const override;

  // This is non-recursive for grouped tab collection as it does not contain
  // another collection.
  std::optional<size_t> GetIndexOfTabRecursive(
      const TabModel* tab_model) const override;

  // This is nullopt as grouped tab collection does not contain another
  // collection.
  std::optional<size_t> GetIndexOfCollection(
      TabCollection* collection) const override;

  std::unique_ptr<TabModel> MaybeRemoveTab(TabModel* tab_model) override;

  // This is the same as number of tabs the group contains as grouped tab
  // collection does not contain another collection.
  size_t ChildCount() const override;

  // TabCollection interface methods that are currently not supported by the
  // collection.
  std::unique_ptr<TabCollection> MaybeRemoveCollection(
      TabCollection* collection) override;

  TabCollectionStorage* GetTabCollectionStorageForTesting() {
    return impl_.get();
  }

 private:
  // The group identifier of this collection.
  const tab_groups::TabGroupId group_id_;

  // Underlying implementation for the storage of children.
  const std::unique_ptr<TabCollectionStorage> impl_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_TAB_COLLECTION_H_
