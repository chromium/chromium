// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/node_id.h"

#include "base/strings/string_number_conversions.h"

namespace tabs_api {

bool operator==(const NodeId& a, const NodeId& b) {
  return a.Type() == b.Type() && a.Id() == b.Id();
}

NodeId NodeId::FromTabHandle(const tabs::TabHandle& handle) {
  return NodeId(Type::kContent, base::NumberToString(handle.raw_value()));
}

NodeId NodeId::FromTabCollectionHandle(
    const tabs::TabCollectionHandle& handle) {
  return NodeId(Type::kCollection, base::NumberToString(handle.raw_value()));
}

std::optional<tabs::TabHandle> NodeId::ToTabHandle() const {
  if (type_ != Type::kContent) {
    return std::nullopt;
  }
  int32_t handle_id;
  if (!base::StringToInt(id_, &handle_id)) {
    return std::nullopt;
  }
  return tabs::TabHandle(handle_id);
}

std::optional<tabs::TabCollectionHandle> NodeId::ToTabCollectionHandle() const {
  if (type_ != Type::kCollection) {
    return std::nullopt;
  }
  int32_t handle_id;
  if (!base::StringToInt(id_, &handle_id)) {
    return std::nullopt;
  }
  return tabs::TabCollectionHandle(handle_id);
}

}  // namespace tabs_api
