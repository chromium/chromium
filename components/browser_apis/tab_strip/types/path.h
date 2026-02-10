// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_PATH_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_PATH_H_

#include <vector>

#include "components/browser_apis/tab_strip/types/node_id.h"

namespace tabs_api {

// Path represents an absolute sequence of NodeIds (collections) from the root
// to the immediate parent of a node.
class Path {
 public:
  Path();
  explicit Path(std::vector<tabs_api::NodeId> components);
  ~Path();

  Path(const Path&);
  Path(Path&&) noexcept;
  Path& operator=(const Path&);
  Path& operator=(Path&&) noexcept;

  const std::vector<tabs_api::NodeId>& components() const {
    return components_;
  }

  friend bool operator==(const Path& a, const Path& b);

 private:
  std::vector<tabs_api::NodeId> components_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_PATH_H_
