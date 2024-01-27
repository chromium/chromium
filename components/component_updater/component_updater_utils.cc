// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_utils.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/version.h"
#include "components/component_updater/component_updater_service.h"

namespace component_updater {

std::optional<ComponentRegistration> GetComponent(
    const base::flat_map<std::string, ComponentRegistration>& components,
    const std::string& id) {
  const auto it = components.find(id);
  if (it != components.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<std::optional<ComponentRegistration>> GetCrxComponents(
    const base::flat_map<std::string, ComponentRegistration>&
        registered_components,
    const std::vector<std::string>& ids) {
  std::vector<std::optional<ComponentRegistration>> components;
  for (const auto& id : ids) {
    components.push_back(GetComponent(registered_components, id));
  }
  return components;
}

}  // namespace component_updater
