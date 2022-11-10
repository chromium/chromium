// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {
struct ComponentRegistration;

absl::optional<ComponentRegistration> GetComponent(
    const base::flat_map<std::string, ComponentRegistration>& components,
    const std::string& id);

std::vector<absl::optional<ComponentRegistration>> GetCrxComponents(
    const base::flat_map<std::string, ComponentRegistration>&
        registered_components,
    const std::vector<std::string>& ids);

// A helper function to delete the path and directory for removed compoennts.
void DeleteFilesAndParentDirectory(const base::FilePath& file_path);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_UTILS_H_
