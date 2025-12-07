// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_POSITION_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_POSITION_H_

#include <optional>

#include "components/browser_apis/tab_strip/types/node_id.h"

namespace tabs_api {

// Position is an ephemeral object that should not be saved nor act as an
// identifier. It is purely used in this API to determine the position within
// the TabstripModel.
class Position {
 public:
  Position();
  explicit Position(size_t index);
  Position(size_t index, std::optional<tabs_api::NodeId> parent_id);
  ~Position();

  Position(const Position&);
  Position(Position&&) noexcept;
  Position& operator=(const Position&);
  Position& operator=(Position&&) noexcept;

  const std::optional<tabs_api::NodeId>& parent_id() const {
    return parent_id_;
  }
  size_t index() const { return index_; }

  friend bool operator==(const Position& a, const Position& b);

 private:
  size_t index_;
  std::optional<tabs_api::NodeId> parent_id_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_POSITION_H_
