// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/node_id.h"

#include "base/strings/string_number_conversions.h"

namespace tabs_api {

bool operator==(const NodeId& a, const NodeId& b) {
  return a.Type() == b.Type() && a.Id() == b.Id();
}

NodeId NodeId::FromTabHandle(const tabs::TabHandle& handle) {
  return NodeId(Type::kContent, base::NumberToString(handle.raw_value()));
}

NodeId NodeId::FromTabGroupId(const tab_groups::TabGroupId& group_id) {
  return NodeId(Type::kCollection, group_id.ToString());
}

}  // namespace tabs_api
