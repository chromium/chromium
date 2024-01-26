// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/update_delta.h"

namespace offline_items_collection {

// static
std::optional<UpdateDelta> UpdateDelta::MergeUpdates(
    const std::optional<UpdateDelta>& update1,
    const std::optional<UpdateDelta>& update2) {
  if (!update1.has_value())
    return update2;

  if (!update2.has_value())
    return update1;

  UpdateDelta merged;
  merged.state_changed = update1->state_changed || update2->state_changed;
  merged.visuals_changed = update1->visuals_changed || update2->visuals_changed;
  return merged;
}

UpdateDelta::UpdateDelta() : state_changed(true), visuals_changed(false) {}

UpdateDelta::UpdateDelta(const UpdateDelta& other) = default;

UpdateDelta::~UpdateDelta() = default;

}  // namespace offline_items_collection
