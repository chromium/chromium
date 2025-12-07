// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/sessions_hierarchy.h"

#include <sstream>

namespace fake_server {

SessionsHierarchy::SessionsHierarchy() = default;

SessionsHierarchy::SessionsHierarchy(const SessionsHierarchy& other) = default;

SessionsHierarchy::SessionsHierarchy(
    std::initializer_list<std::multiset<std::string>> windows)
    : windows_(windows) {}

SessionsHierarchy::~SessionsHierarchy() = default;

void SessionsHierarchy::AddWindow(const std::string& tab) {
  windows_.insert({tab});
}

void SessionsHierarchy::AddWindow(const std::multiset<std::string>& tabs) {
  windows_.insert(tabs);
}

std::string SessionsHierarchy::ToString() const {
  std::stringstream output;
  output << "{";
  bool first_window = true;
  for (const Window& window : windows_) {
    if (!first_window) {
      output << ",";
    }
    output << "{";
    first_window = false;

    bool first_tab = true;
    for (const std::string& tab : window) {
      if (!first_tab) {
        output << ",";
      }
      output << tab;
      first_tab = false;
    }
    output << "}";
  }
  output << "}";
  return output.str();
}

bool SessionsHierarchy::Equals(const SessionsHierarchy& other) const {
  return windows_ == other.windows_;
}

}  // namespace fake_server
