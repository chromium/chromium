// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/resources/input_overlay_resources_util.h"

#include <map>

#include "components/arc/grit/input_overlay_resources.h"
#include "components/arc/input_overlay/actions/action_tap_key.h"
#include "components/arc/input_overlay/actions/dependent_position.h"
#include "components/arc/input_overlay/actions/position.h"

namespace arc {

absl::optional<int> GetInputOverlayResourceId(const std::string& package_name) {
  static std::map<std::string, int> resource_id_map = {
      {"org.chromium.arc.testapp.inputoverlay",
       IDR_IO_ORG_CHROMIUM_ARC_TESTAPP_INPUTOVERLAY},
      {"com.blackpanther.ninjaarashi2", IDR_IO_COM_BLACKPANTHER_NINJAARASHI2},
  };

  auto it = resource_id_map.find(package_name);
  if (it != resource_id_map.end())
    return absl::optional<int>(it->second);
  return absl::optional<int>();
}

absl::optional<std::vector<std::unique_ptr<input_overlay::Action>>>
ParseJsonToActions(aura::Window* window, const base::Value& root) {
  // Parse tap action if exists.
  const base::Value* tap_act_val = root.FindKey(input_overlay::kTapAction);
  std::vector<std::unique_ptr<input_overlay::Action>> actions;
  if (tap_act_val) {
    const base::Value* keyboard_act_list =
        tap_act_val->FindListKey(input_overlay::kKeyboard);
    if (keyboard_act_list && keyboard_act_list->is_list()) {
      for (const base::Value& val : keyboard_act_list->GetList()) {
        std::unique_ptr<input_overlay::Action> action =
            std::make_unique<input_overlay::ActionTapKey>(window);
        bool succeed = action->ParseFromJson(val);
        if (succeed)
          actions.emplace_back(std::move(action));
      }
    }
  }
  // TODO: parse more actions.
  if (actions.empty())
    return absl::nullopt;
  return absl::make_optional(std::move(actions));
}

absl::optional<std::vector<std::unique_ptr<input_overlay::Position>>>
ParseLocation(const base::Value& position) {
  std::vector<std::unique_ptr<input_overlay::Position>> positions;
  for (const base::Value& val : position.GetList()) {
    auto* type = val.FindStringKey(input_overlay::kType);
    if (!type) {
      LOG(ERROR) << "There must be position type for each location.";
      return absl::nullopt;
    }
    size_t size = positions.size();
    if (type->compare(input_overlay::kPosition) == 0) {
      positions.emplace_back(std::make_unique<input_overlay::Position>());
    } else if (type->compare(input_overlay::kDependentPosition) == 0) {
      positions.emplace_back(
          std::make_unique<input_overlay::DependentPosition>());
    }

    if (positions.size() == size) {
      LOG(ERROR) << "There is position with unknown type: " << *type;
      return absl::nullopt;
    }

    bool succeed = positions.back()->ParseFromJson(val);
    if (!succeed) {
      LOG(ERROR) << "Position is parsed incorrectly on type: " << *type;
      return absl::nullopt;
    }
  }

  if (positions.empty())
    return absl::nullopt;
  return absl::make_optional(std::move(positions));
}

}  // namespace arc
