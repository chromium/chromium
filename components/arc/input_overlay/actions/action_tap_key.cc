// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/actions/action_tap_key.h"

#include "components/arc/input_overlay/actions/action.h"
#include "components/arc/input_overlay/resources/input_overlay_resources_util.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace arc {
namespace input_overlay {

ActionTapKey::ActionTapKey(aura::Window* window) : Action(window) {}

ActionTapKey::~ActionTapKey() = default;

bool ActionTapKey::ParseFromJson(const base::Value& value) {
  Action::ParseFromJson(value);
  if (locations_.size() == 0) {
    LOG(ERROR) << "Require at least one location for tap key action {" << name_
               << "}.";
    return false;
  }
  const std::string* key = value.FindStringKey(kKey);
  if (!key) {
    LOG(ERROR) << "Require key code for tap key action {" << name_ << "}.";
    return false;
  }
  key_ = ui::KeycodeConverter::CodeStringToDomCode(*key);
  if (key_ == ui::DomCode::NONE) {
    LOG(ERROR) << "Tap key code is invalid for tap key action {" << name_
               << "}. It should be similar to {KeyA}, but got {" << *key
               << "}.";
    return false;
  }
  return true;
}

}  // namespace input_overlay
}  // namespace arc
