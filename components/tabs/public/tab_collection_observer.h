// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_OBSERVER_H_
#define COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_OBSERVER_H_

#include "base/observer_list_types.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

class TabCollectionObserver : public base::CheckedObserver {
 public:
  struct NodeData {
    const TabCollection::Position position;
    const TabCollection::NodeHandle handle;
  };

  // Notifies that tabs and collections are added starting at position.
  // TODO(crbug.com/435179292): Remove insert_from_detached bool and pausing by
  // proper selection model synchronization with collection updates.
  // `insert_from_detached` is used to signal the client to read the whole
  // subtree.
  virtual void OnChildrenAdded(const TabCollection::Position& position,
                               const TabCollectionNodes& handles,
                               bool insert_from_detached) {}

  // Notifies that tabs and collections are removed. `position` represents
  // the start of the block after which all children are removed.
  virtual void OnChildrenRemoved(const TabCollection::Position& position,
                                 const TabCollectionNodes& handles) {}

  // Notifies that a tab or collection is moved to a position. node_data
  // contains the src information.
  virtual void OnChildMoved(const TabCollection::Position& to_position,
                            const NodeData& node_data) {}
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_OBSERVER_H_
