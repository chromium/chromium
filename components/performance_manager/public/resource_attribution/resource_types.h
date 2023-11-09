// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_RESOURCE_TYPES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_RESOURCE_TYPES_H_

#include "base/containers/enum_set.h"

namespace performance_manager::resource_attribution {

// Types of resources that Resource Attribution can measure.
enum class ResourceType {
  // CPU usage, measured in time spent on CPU.
  kCPUTime,
};

using ResourceTypeSet =
    base::EnumSet<ResourceType, ResourceType::kCPUTime, ResourceType::kCPUTime>;

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_RESOURCE_TYPES_H_
