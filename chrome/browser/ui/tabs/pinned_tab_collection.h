// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PINNED_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_PINNED_TAB_COLLECTION_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/tab_collection.h"

namespace tabs {

class TabModel;
class TabCollectionStorage;

class PinnedTabCollection : public TabCollection {
 public:
  PinnedTabCollection();
  ~PinnedTabCollection() override;
  PinnedTabCollection(const PinnedTabCollection&) = delete;
  PinnedTabCollection& operator=(const PinnedTabCollection&) = delete;

  // Adds a `tab_model` to the `impl_` at a particular index.
  void AddTab(std::unique_ptr<TabModel> tab_model, size_t index);

  // Appends a `tab_model` to the end of a `impl_`.
  void AppendTab(std::unique_ptr<TabModel> tab_model);

  // Moves a `tab_model` to the `dst_index` within `impl_`.
  void MoveTab(TabModel* tab_model, size_t dst_index);

  // Removes and cleans the `tab_model`.
  void CloseTab(TabModel* tab_model);

  // Returns the tab at a direct child index in this collection. If the index is
  // invalid it returns nullptr.
  tabs::TabModel* GetTabAtIndex(size_t index) const;

  // TabCollection:
  bool ContainsTab(TabModel* tab_model) const override;

  // This is non-recursive for pinned tab collection as it does not contain
  // another collection.
  bool ContainsTabRecursive(TabModel* tab_model) const override;

  // This is false as pinned tab collection does not contain another collection.
  bool ContainsCollection(TabCollection* collection) const override;

  // This is non-recursive for pinned tab collection as it does not contain
  // another collection.
  std::optional<size_t> GetIndexOfTabRecursive(
      const TabModel* tab_model) const override;

  // This is nullopt as pinned tab collection does not contain another
  // collection.
  std::optional<size_t> GetIndexOfCollection(
      TabCollection* collection) const override;

  std::unique_ptr<TabModel> MaybeRemoveTab(TabModel* tab_model) override;

  // This is the same as number of tabs `impl_` contains as pinned tab
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
  // Underlying implementation for the storage of children.
  std::unique_ptr<TabCollectionStorage> impl_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PINNED_TAB_COLLECTION_H_
