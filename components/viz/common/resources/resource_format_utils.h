// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_

#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/viz_resource_format_export.h"

namespace viz {

// |use_angle_rgbx_format| should be true when the GL_ANGLE_rgbx_internal_format
// extension is available.
VIZ_RESOURCE_FORMAT_EXPORT unsigned int TextureStorageFormat(
    ResourceFormat format,
    bool use_angle_rgbx_format);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
