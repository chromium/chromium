// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_NODE_ID_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_NODE_ID_H_

#include <string>
#include <string_view>

#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs_api {

// A discrete object representing the id of a node in the tab tree.
// Clients should never construct their own id instance and should only
// use the IDs returned by the tab service.
class NodeId {
 public:
  enum class Type {
    kInvalid,
    // kContent ids are mapped to TabHandle::Handle, which represents an
    // int32_t number.
    kContent,
    kCollection,
  };

  NodeId() : NodeId(Type::kInvalid, "") {}
  NodeId(enum Type type, std::string_view id) : type_(type), id_(id) {}
  ~NodeId() = default;

  static NodeId FromTabHandle(const tabs::TabHandle& handle);
  static NodeId FromTabCollectionHandle(
      const tabs::TabCollectionHandle& handle);

  std::optional<tabs::TabHandle> ToTabHandle() const;
  std::optional<tabs::TabCollectionHandle> ToTabCollectionHandle() const;

  std::string_view Id() const { return id_; }

  Type Type() const { return type_; }

  // Two node ids are equal iff they represent the same underlying resource
  // (denoted by the type) and they have the same id.
  friend bool operator==(const NodeId& a, const NodeId& b);

 private:
  enum Type type_;
  std::string id_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_NODE_ID_H_
