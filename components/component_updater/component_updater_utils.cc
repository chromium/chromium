// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_utils.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/version.h"
#include "components/component_updater/component_updater_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace component_updater {

absl::optional<ComponentRegistration> GetComponent(
    const base::flat_map<std::string, ComponentRegistration>& components,
    const std::string& id) {
  const auto it = components.find(id);
  if (it != components.end())
    return it->second;
  return absl::nullopt;
}

std::vector<absl::optional<ComponentRegistration>> GetCrxComponents(
    const base::flat_map<std::string, ComponentRegistration>&
        registered_components,
    const std::vector<std::string>& ids) {
  std::vector<absl::optional<ComponentRegistration>> components;
  for (const auto& id : ids)
    components.push_back(GetComponent(registered_components, id));
  return components;
}

}  // namespace component_updater
