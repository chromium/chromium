// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_NODE_INTERFACE_H_
#define COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_NODE_INTERFACE_H_

namespace tabs {

class TabCollection;
class TabInterface;

// A generic node in a tab collection tree. This is essentially a `std::variant`
// wrapper that abstracts away the underlying concete types and their memory
// management models.
//
// The need for this class arises from the fact that on Desktop tabs are stored
// as `std::unique_ptr<TabModel>` whereas on Android we use `TabAndroid*`. The
// semantics of interacting with a unique_ptr and raw ptr are sufficiently
// different that a simple typedef is insufficient to resolve the difference and
// so a wrapper indirection is required.
class TabCollectionNodeInterface {
 public:
  enum class Type {
    kTabInterface,
    kTabCollection,
  };

  TabCollectionNodeInterface() = default;
  virtual ~TabCollectionNodeInterface() = default;
  TabCollectionNodeInterface(const TabCollectionNodeInterface& other) = delete;
  void operator=(const TabCollectionNodeInterface& other) = delete;

  // Returns the `Type` corresponding to the object stored in this collection.
  virtual Type GetType() const = 0;

  // Returns the stored `TabInterface`. Crashes if `Type` is not
  // `kTabInterface`.
  virtual TabInterface* GetTabInterface() const = 0;

  // Returns the stored `TabCollection`. Crashes if `Type` is not
  // `kTabCollection`.
  virtual TabCollection* GetTabCollection() const = 0;
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_NODE_INTERFACE_H_
