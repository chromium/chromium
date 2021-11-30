// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_utils.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/update_client/update_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace component_updater {

absl::optional<update_client::CrxComponent> GetComponent(
    const base::flat_map<std::string, update_client::CrxComponent>& components,
    const std::string& id) {
  const auto it = components.find(id);
  if (it != components.end())
    return it->second;
  return absl::nullopt;
}

std::vector<absl::optional<update_client::CrxComponent>> GetCrxComponents(
    const base::flat_map<std::string, update_client::CrxComponent>&
        registered_components,
    const std::vector<std::string>& ids) {
  std::vector<absl::optional<update_client::CrxComponent>> components;
  for (const auto& id : ids)
    components.push_back(GetComponent(registered_components, id));
  return components;
}

}  // namespace component_updater
