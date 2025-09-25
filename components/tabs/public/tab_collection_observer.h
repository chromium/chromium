// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_OBSERVER_H_
#define COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_OBSERVER_H_

#include <map>
#include <variant>
#include <vector>

#include "base/observer_list_types.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

using ChildHandle = std::variant<tabs::TabCollectionHandle, tabs::TabHandle>;
using ChildrenHandleVector =
    std::vector<std::variant<tabs::TabCollectionHandle, tabs::TabHandle>>;

class TabCollectionObserver : public base::CheckedObserver {
 public:
  // the parent collection and direct index within the parent collection for a
  // child node. This uniquely determines the position of a node in the tree.
  struct Position {
    TabCollection::Handle parent_id;
    size_t index;
  };

  // Notifies that tabs and collections are added starting at position.
  virtual void OnChildrenAdded(const ChildrenHandleVector& ids,
                               Position position) {}

  // Notifies that tabs and collections are removed starting at position.
  virtual void OnChildrenRemoved(const ChildrenHandleVector& ids) {}

  // Notifies that tabs and collections are moved to a block starting at
  // position.
  virtual void OnChildrenMoved(const ChildrenHandleVector& ids,
                               Position position) {}
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_OBSERVER_H_
