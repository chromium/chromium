// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENT_LOADER_POLICY_FORWARD_H_
#define COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENT_LOADER_POLICY_FORWARD_H_

#include <memory>
#include <vector>

namespace component_updater {
class ComponentLoaderPolicy;

using ComponentLoaderPolicyVector =
    std::vector<std::unique_ptr<ComponentLoaderPolicy>>;

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENT_LOADER_POLICY_FORWARD_H_
