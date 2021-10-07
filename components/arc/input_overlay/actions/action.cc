// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/actions/action.h"
#include "components/arc/input_overlay/resources/input_overlay_resources_util.h"

namespace arc {
namespace input_overlay {

Action::Action(aura::Window* window) : target_window_(window) {}

Action::~Action() = default;

bool Action::ParseFromJson(const base::Value& value) {
  // Name can be empty.
  auto* name = value.FindStringKey(kName);
  if (name) {
    name_ = *name;
  }
  // Location can be empty for mouse related actions.
  const base::Value* position = value.FindKey(kLocation);
  if (position) {
    auto parsed_pos = ParseLocation(*position);
    if (parsed_pos) {
      std::move(parsed_pos->begin(), parsed_pos->end(),
                std::back_inserter(locations_));
    }
  }
  return true;
}

}  // namespace input_overlay
}  // namespace arc
