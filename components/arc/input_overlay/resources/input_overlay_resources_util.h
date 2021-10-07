// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INPUT_OVERLAY_RESOURCES_INPUT_OVERLAY_RESOURCES_UTIL_H_
#define COMPONENTS_ARC_INPUT_OVERLAY_RESOURCES_INPUT_OVERLAY_RESOURCES_UTIL_H_

#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/arc/input_overlay/actions/action.h"
#include "components/arc/input_overlay/touch_injector.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {
namespace input_overlay {
// Key strings in JSON file.
// About tap action.
constexpr char kTapAction[] = "tap";
constexpr char kKeyboard[] = "keyboard";
constexpr char kName[] = "name";
constexpr char kKey[] = "key";

// About position.
constexpr char kLocation[] = "location";
constexpr char kPositions[] = "position";
constexpr char kAnchor[] = "anchor";
constexpr char kAnchorToTarget[] = "anchor_to_target";
}  // namespace input_overlay

// Get the resource ID of the input overlay JSON file by the associated package
// name.
absl::optional<int> GetInputOverlayResourceId(const std::string& package_name);

// Parse Json to different types of actions.
absl::optional<std::vector<std::unique_ptr<input_overlay::Action>>>
ParseJsonToActions(aura::Window* window, const base::Value& root);

// Parse Json to different types of positions.
absl::optional<std::vector<std::unique_ptr<input_overlay::Position>>>
ParseLocation(const base::Value& position);

}  // namespace arc

#endif  // COMPONENTS_ARC_INPUT_OVERLAY_RESOURCES_INPUT_OVERLAY_RESOURCES_UTIL_H_
