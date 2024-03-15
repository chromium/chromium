// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_COLLECTION_H_
#define CHROME_BROWSER_UI_TABS_TAB_COLLECTION_H_

#include <cstddef>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"

namespace tabs {

class TabModel;

class TabCollection {
 public:
  TabCollection() = default;
  virtual ~TabCollection() = default;
  TabCollection(const TabCollection&) = delete;
  TabCollection& operator=(const TabCollection&) = delete;

  // Returns true if the tab model is a direct child of the collection.
  virtual bool ContainsTab(TabModel* tab_model) const = 0;

  // Returns true if the tab collection tree contains the tab.
  virtual bool ContainsTabRecursive(TabModel* tab_model) const = 0;

  // Returns true is the tab collection contains the collection. This is a
  // non-recursive check.
  virtual bool ContainsCollection(TabCollection* collection) const = 0;

  // Recursively get the index of the tab_model among all the leaf tab_models.
  virtual std::optional<size_t> GetIndexOfTabRecursive(
      TabModel* tab_model) const = 0;

  // Non-recursively get the index of a collection.
  virtual std::optional<size_t> GetIndexOfCollection(
      TabCollection* collection) const = 0;

  // Total number of children that directly have this collection as their
  // parent.
  virtual size_t ChildCount() const = 0;

  // Total number of tabs the collection contains.
  virtual size_t TabCountRecursive() const = 0;

  // Removes the tab if it is a direct child of this collection. This is then
  // returned to the caller as an unique_ptr. If the tab is not present it will
  // return a nullptr.
  [[nodiscard]] virtual std::unique_ptr<TabModel> MaybeRemoveTab(
      TabModel* tab) = 0;

  // Removes the collection if it is a direct child of this collection. This is
  // then returned to the caller as an unique_ptr. If the collection is not
  // present it will return a nullptr.
  [[nodiscard]] virtual std::unique_ptr<TabCollection> MaybeRemoveCollection(
      TabCollection* collection) = 0;

  TabCollection* GetParentCollection() const { return parent_; }

  // This should be called either when this collection is added to another
  // collection or it is removed from another collection. The child collection
  // should not try to call this internally and set its parent.
  void OnReparented(TabCollection* new_parent) { parent_ = new_parent; }

 protected:
  base::PassKey<TabCollection> GetPassKey() {
    return base::PassKey<TabCollection>();
  }

 private:
  raw_ptr<TabCollection> parent_ = nullptr;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_COLLECTION_H_
