// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/common/element_id.h"

#include <ostream>
#include <sstream>

namespace record_replay {

ElementId::ElementId(DomNodeId dom_node_id) : dom_node_id_(dom_node_id) {}

ElementId::ElementId(const ElementId&) = default;

ElementId& ElementId::operator=(const ElementId&) = default;

ElementId::~ElementId() = default;

std::string ElementId::ToString() const {
  std::ostringstream ss;
  ss << dom_node_id_;
  return ss.str();
}

bool operator==(const ElementId& lhs, const ElementId& rhs) = default;
auto operator<=>(const ElementId& lhs, const ElementId& rhs) = default;

std::ostream& operator<<(std::ostream& os, const ElementId& element_id) {
  return os << element_id.ToString();
}

}  // namespace record_replay
