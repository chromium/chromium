// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/sessions_hierarchy.h"

#include <sstream>

namespace fake_server {

SessionsHierarchy::SessionsHierarchy() {}

SessionsHierarchy::SessionsHierarchy(const SessionsHierarchy& other) = default;

SessionsHierarchy::SessionsHierarchy(
    std::initializer_list<std::multiset<std::string>> windows)
    : windows_(windows) {}

SessionsHierarchy::~SessionsHierarchy() {}

void SessionsHierarchy::AddWindow(const std::string& tab) {
  windows_.insert({tab});
}

void SessionsHierarchy::AddWindow(const std::multiset<std::string>& tabs) {
  windows_.insert(tabs);
}

std::string SessionsHierarchy::ToString() const {
  std::stringstream output;
  output << "{";
  for (auto window_it = windows_.begin(); window_it != windows_.end();
       ++window_it) {
    if (window_it != windows_.begin())
      output << ",";
    output << "{";

    Window window = *window_it;
    for (auto tab_it = window.begin(); tab_it != window.end(); ++tab_it) {
      if (tab_it != window.begin())
        output << ",";
      output << *tab_it;
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
