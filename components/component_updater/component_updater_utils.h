// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"

namespace component_updater {
struct ComponentRegistration;

std::optional<ComponentRegistration> GetComponent(
    const base::flat_map<std::string, ComponentRegistration>& components,
    const std::string& id);

std::vector<std::optional<ComponentRegistration>> GetCrxComponents(
    const base::flat_map<std::string, ComponentRegistration>&
        registered_components,
    const std::vector<std::string>& ids);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_
