// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_

#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/viz_resource_format_export.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_types.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "third_party/vulkan/include/vulkan/vulkan.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include <dawn/dawncpp.h>
#endif

namespace viz {

VIZ_RESOURCE_FORMAT_EXPORT SkColorType
ResourceFormatToClosestSkColorType(bool gpu_compositing, ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT int BitsPerPixel(ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT bool HasAlpha(ResourceFormat format);

// The following functions use unsigned int instead of GLenum, since including
// third_party/khronos/GLES2/gl2.h causes redefinition errors as
// macros/functions defined in it conflict with macros/functions defined in
// ui/gl/gl_bindings.h. See http://crbug.com/512833 for more information.
VIZ_RESOURCE_FORMAT_EXPORT unsigned int GLDataType(ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT unsigned int GLDataFormat(ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT unsigned int GLInternalFormat(ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT unsigned int GLCopyTextureInternalFormat(
    ResourceFormat format);

// Returns the pixel format of the resource when mapped into client-side memory.
// Returns a default value when IsGpuMemoryBufferFormatSupported() returns false
// for a given format, as in this case the resource will not be mapped into
// client-side memory, and the returned value is not used.
VIZ_RESOURCE_FORMAT_EXPORT gfx::BufferFormat BufferFormat(
    ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT bool IsResourceFormatCompressed(
    ResourceFormat format);
VIZ_RESOURCE_FORMAT_EXPORT unsigned int TextureStorageFormat(
    ResourceFormat format);

// Returns whether the format can be used with GpuMemoryBuffer texture storage,
// allocated through TexStorage2DImageCHROMIUM.
VIZ_RESOURCE_FORMAT_EXPORT bool IsGpuMemoryBufferFormatSupported(
    ResourceFormat format);

// Returns whether the format can be used as a software bitmap for export to the
// display compositor.
VIZ_RESOURCE_FORMAT_EXPORT bool IsBitmapFormatSupported(ResourceFormat format);

VIZ_RESOURCE_FORMAT_EXPORT ResourceFormat
GetResourceFormat(gfx::BufferFormat format);

VIZ_RESOURCE_FORMAT_EXPORT bool GLSupportsFormat(ResourceFormat format);

#if BUILDFLAG(ENABLE_VULKAN)
VIZ_RESOURCE_FORMAT_EXPORT VkFormat ToVkFormat(ResourceFormat format);
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
VIZ_RESOURCE_FORMAT_EXPORT dawn::TextureFormat ToDawnFormat(
    ResourceFormat format);
#endif

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_UTILS_H_
