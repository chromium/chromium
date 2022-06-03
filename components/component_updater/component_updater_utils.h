// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace update_client {
struct CrxComponent;
}  // namespace update_client

namespace component_updater {

absl::optional<update_client::CrxComponent> GetComponent(
    const base::flat_map<std::string, update_client::CrxComponent>& components,
    const std::string& id);

std::vector<absl::optional<update_client::CrxComponent>> GetCrxComponents(
    const base::flat_map<std::string, update_client::CrxComponent>&
        registered_components,
    const std::vector<std::string>& ids);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_
