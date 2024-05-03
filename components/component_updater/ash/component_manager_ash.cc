// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/ash/component_manager_ash.h"

namespace component_updater {

CompatibleComponentInfo::CompatibleComponentInfo() = default;

CompatibleComponentInfo::CompatibleComponentInfo(
    const base::FilePath& path_in,
    const std::optional<base::Version>& version_in)
    : path(path_in), version(version_in) {}

CompatibleComponentInfo::CompatibleComponentInfo(
    CompatibleComponentInfo&& lhs) = default;
CompatibleComponentInfo& CompatibleComponentInfo::operator=(
    CompatibleComponentInfo&& lhs) = default;

CompatibleComponentInfo::~CompatibleComponentInfo() = default;

ComponentManagerAsh::ComponentManagerAsh() = default;
ComponentManagerAsh::~ComponentManagerAsh() = default;

}  // namespace component_updater
