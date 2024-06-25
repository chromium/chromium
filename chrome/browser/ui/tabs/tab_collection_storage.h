// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_COLLECTION_STORAGE_H_
#define CHROME_BROWSER_UI_TABS_TAB_COLLECTION_STORAGE_H_

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "base/memory/raw_ref.h"

namespace tabs {

class TabModel;
class TabCollection;

using ChildrenVector = std::vector<
    std::variant<std::unique_ptr<TabCollection>, std::unique_ptr<TabModel>>>;

// Provides reusable functionality useful to most TabCollections for storing
// and manipulating a vector of child tabs and collections.
// Note that a TabCollectionStorage *is not* a TabCollection, and it
// does not have:
// - a parent TabCollection: a TabCollectionStorage doesn't live in the
// collection tree
// - MaybeRemoveTab/MaybeRemoveCollection - the storage layer doesn't get to say
// no
class TabCollectionStorage final {
 public:
  explicit TabCollectionStorage(TabCollection& owner);
  ~TabCollectionStorage();
  TabCollectionStorage(const TabCollectionStorage&) = delete;
  TabCollectionStorage& operator=(const TabCollectionStorage&) = delete;

  // Inserts a Tab into the TabCollectionStorage. The `index` represents the
  // position in the direct children vector (non-recursive).
  TabModel* AddTab(std::unique_ptr<TabModel> tab_model, size_t index);

  // Moves a tab already within this TabCollectionStorage to `dst_index`. Shifts
  // other tabs and collections in the collection as needed. Will check if index
  // is OOB.
  void MoveTab(TabModel* tab_model, size_t dst_index);

  // Removes `tab_model` from storage and returns it to the caller.
  [[nodiscard]] std::unique_ptr<TabModel> RemoveTab(TabModel* tab_model);

  // Removes a Tab in the TabCollectionStorage and frees the memory.
  void CloseTab(TabModel* tab_model);

  // Inserts a TabCollection into the TabCollectionStorage. The `index`
  // represents the position in the direct children vector (non-recursive).
  TabCollection* AddCollection(std::unique_ptr<TabCollection> collection,
                               size_t index);

  // Moves a collection already within this TabCollectionStorage to a new
  // `index` which is the destination before its move. Shifts other tabs
  // and collections in the collection as needed. Will check if index
  // is OOB.
  void MoveCollection(TabCollection* collection, size_t dst_index);

  // Removes a TabCollection from storage and returns it to the caller. If no
  // collection is found, returns nullptr.
  [[nodiscard]] std::unique_ptr<TabCollection> RemoveCollection(
      TabCollection* collection);

  // Closes a stored TabCollection, and all tabs and collections it recursively
  // contains. This frees the memory as well.
  void CloseCollection(TabCollection* collection);

  // Returns true if the `tab_model` is owned by the `children_`.
  bool ContainsTab(TabModel* tab_model) const;

  // Returns true if the `tab_collection` is owned by the `children_`.
  bool ContainsCollection(TabCollection* tab_collection) const;

  // Returns the index of the `tab_model` in `children_`. It returns a nullopt
  // if the `tab_model` is not present in the `children_`.
  std::optional<size_t> GetIndexOfTab(const TabModel* tab_model) const;

  // Returns the tab at a direct index if the child at the direct index is a
  // tab.
  TabModel* GetTabAtIndex(size_t index) const;

  // Returns the index of the `tab_collection` in `children_`. It returns a
  // nullopt if the `tab_collection` is not present in the `children_`.
  std::optional<size_t> GetIndexOfCollection(
      TabCollection* tab_collection) const;

  // Returns the total number of elements stored in `children_`. This is
  // equivalent to the sum of TabModel and TabCollection present in `children_`.
  size_t GetChildrenCount() const;

  // Returns read only version of `children_` for clients to query
  // information about the individual elements.
  const ChildrenVector& GetChildren() const { return children_; }

 private:
  // This is where the actual storage is present. `children_` is a vector of
  // either a `TabModel`or a `TabCollection` and has ownership of the elements.
  ChildrenVector children_;

  // The collection that owns this TabCollectionStorage.
  const raw_ref<TabCollection> owning_collection_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_COLLECTION_STORAGE_H_
