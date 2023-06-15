// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_

#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/viz_resource_format_export.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include <vulkan/vulkan_core.h>
#endif

namespace viz {

// The following functions use unsigned int instead of GLenum, since including
// third_party/khronos/GLES2/gl2.h causes redefinition errors as
// macros/functions defined in it conflict with macros/functions defined in
// ui/gl/gl_bindings.h. See http://crbug.com/512833 for more information.
VIZ_RESOURCE_FORMAT_EXPORT unsigned int GLDataType(ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT unsigned int GLDataFormat(ResourceFormat format);

// |use_angle_rgbx_format| should be true when the GL_ANGLE_rgbx_internal_format
// extension is available.
VIZ_RESOURCE_FORMAT_EXPORT unsigned int TextureStorageFormat(
    ResourceFormat format,
    bool use_angle_rgbx_format);

#if BUILDFLAG(ENABLE_VULKAN)
VIZ_RESOURCE_FORMAT_EXPORT bool HasVkFormat(ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT VkFormat ToVkFormat(ResourceFormat format);
#endif

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
