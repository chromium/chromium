// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_SETTINGS_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_SETTINGS_H_

#include <stddef.h>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// ResourceSettings contains all the settings that are needed to create a
// ResourceProvider.
class VIZ_COMMON_EXPORT ResourceSettings {
 public:
  ResourceSettings();
  ResourceSettings(const ResourceSettings& other);
  ResourceSettings& operator=(const ResourceSettings& other);
  ~ResourceSettings();

  bool use_gpu_memory_buffer_resources = false;
  // TODO(crbug.com/759456): Remove after r16 is used without the flag.
  bool use_r16_texture = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_SETTINGS_H_
