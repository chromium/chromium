// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fusebox_action_mojo_test_utils.h"

#include <ostream>

namespace fusebox_action::mojom {

namespace {
std::string IndentStr(int indent) {
  return std::string(indent, ' ');
}

void PrintImpl(const FuseboxAction& action, int indent, std::ostream* os) {
  std::string ind = IndentStr(indent);
  *os << "FuseboxAction{\n";
  if (action.preselected_tool) {
    *os << ind
        << "  preselected_tool: " << static_cast<int>(*action.preselected_tool)
        << ",\n";
  } else {
    *os << ind << "  preselected_tool: null,\n";
  }
  if (action.preferred_inventory) {
    *os << ind << "  preferred_inventory: "
        << static_cast<int>(*action.preferred_inventory) << ",\n";
  } else {
    *os << ind << "  preferred_inventory: null,\n";
  }
  if (action.preselected_model) {
    *os << ind << "  preselected_model: "
        << static_cast<int>(*action.preselected_model) << ",\n";
  } else {
    *os << ind << "  preselected_model: null,\n";
  }
  if (action.query_action_override) {
    *os << ind << "  query_action_override: "
        << static_cast<int>(*action.query_action_override) << ",\n";
  } else {
    *os << ind << "  query_action_override: null,\n";
  }
  *os << ind << "}";
}

void PrintImpl(const FuseboxActionPtr& action, int indent, std::ostream* os) {
  if (action) {
    PrintImpl(*action, indent, os);
  } else {
    *os << "null";
  }
}
}  // namespace

void PrintTo(const FuseboxAction& action, std::ostream* os) {
  PrintImpl(action, 0, os);
}

void PrintTo(const FuseboxActionPtr& action, std::ostream* os) {
  PrintImpl(action, 0, os);
}

}  // namespace fusebox_action::mojom
