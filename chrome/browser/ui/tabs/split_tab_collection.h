// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_COLLECTION_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/split_tab_id.h"
#include "chrome/browser/ui/tabs/tab_collection.h"
#include "url/gurl.h"

namespace tabs {

class TabModel;
class TabCollectionStorage;

enum class SplitTabLayout { kHorizontal, kVertical };

class SplitTabCollection : public TabCollection {
 public:
  explicit SplitTabCollection(split_tabs::SplitTabId split_id);
  ~SplitTabCollection() override;
  SplitTabCollection(const SplitTabCollection&) = delete;
  SplitTabCollection& operator=(const SplitTabCollection&) = delete;

  // Adds a `tab_model` to the split at a particular index.
  void AddTab(std::unique_ptr<TabModel> tab_model, size_t index);

  // Appends a `tab_model` to the end of the split.
  void AppendTab(std::unique_ptr<TabModel> tab_model);

  // Moves a `tab_model` to the `dst_index` within the split.
  void MoveTab(TabModel* tab_model, size_t dst_index);

  // Removes and cleans the `tab_model`.
  void CloseTab(TabModel* tab_model);

  // Returns the `split_id_` this collection is associated with.
  split_tabs::SplitTabId GetSplitTabId() const { return split_id_; }

  // Returns the tab at a direct child index in this collection. If the index is
  // invalid it returns nullptr.
  tabs::TabModel* GetTabAtIndex(size_t index) const;

  // TabCollection:
  bool ContainsTab(const TabInterface* tab) const override;

  // This is non-recursive for split tab collection as it does not contain
  // another collection.
  bool ContainsTabRecursive(const TabInterface* tab) const override;

  // This is false as split tab collection does not contain another
  // collection.
  bool ContainsCollection(TabCollection* collection) const override;

  // This is non-recursive for split tab collection as it does not contain
  // another collection.
  std::optional<size_t> GetIndexOfTabRecursive(
      const TabInterface* tab) const override;

  // This is nullopt as split tab collection does not contain another
  // collection.
  std::optional<size_t> GetIndexOfCollection(
      TabCollection* collection) const override;

  std::unique_ptr<TabModel> MaybeRemoveTab(TabModel* tab_model) override;

  // This is the same as number of tabs the split contains as split tab
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
  // The split identifier of this collection.
  const split_tabs::SplitTabId split_id_;

  // Underlying implementation for the storage of children.
  const std::unique_ptr<TabCollectionStorage> impl_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_COLLECTION_H_
