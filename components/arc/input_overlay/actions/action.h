// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
#define COMPONENTS_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_

#include <string>

#include "base/values.h"
#include "components/arc/input_overlay/actions/position.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace arc {
namespace input_overlay {

// This is the base touch action which converts other events to touch
// events for input overlay.
class Action {
 public:
  Action(const Action&) = delete;
  Action& operator=(const Action&) = delete;
  virtual ~Action();

  virtual bool ParseFromJson(const base::Value& value);

  const std::string& name() { return name_; }
  const std::vector<std::unique_ptr<Position>>& locations() const {
    return locations_;
  }

 protected:
  explicit Action(aura::Window* window);

  // name_ is basically for debugging and not visible to users.
  std::string name_;
  // Location take turns for each key press if there are more than
  // one location.
  std::vector<std::unique_ptr<Position>> locations_;

  aura::Window* target_window_;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // COMPONENTS_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
