// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_UTILS_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_UTILS_H_

#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/viz_resource_format_export.h"
#include "gpu/vulkan/buildflags.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "ui/gfx/buffer_types.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include <vulkan/vulkan_core.h>
#endif

namespace gpu {
class SharedImageFormatRestrictedSinglePlaneUtilsAccessor;
}  // namespace gpu

namespace media {
class VideoResourceUpdater;
}

namespace viz {

// Returns the closest SkColorType for a given single planar `format`.
//
// NOTE: The formats BGRX_8888, BGR_565 and BGRA_1010102 return a SkColorType
// with R/G channels reversed. This is because from GPU perspective, GL format
// is always RGBA and there is no difference between RGBA/BGRA. Also, these
// formats should not be used for software SkImages/SkSurfaces.
VIZ_RESOURCE_FORMAT_EXPORT SkColorType
ToClosestSkColorType(bool gpu_compositing, SharedImageFormat format);

// Returns the closest SkColorType for a given `format` and `plane_index`. For
// single planar formats (eg. RGBA) the plane_index must be zero and it's
// equivalent to calling function above.
VIZ_RESOURCE_FORMAT_EXPORT SkColorType
ToClosestSkColorType(bool gpu_compositing,
                     SharedImageFormat format,
                     int plane_index);

// Returns the single-plane SharedImageFormat corresponding to `color_type.`
VIZ_RESOURCE_FORMAT_EXPORT SharedImageFormat
SkColorTypeToSinglePlaneSharedImageFormat(SkColorType color_type);

// Returns whether `format`, which must be a single-planar format, can be used
// with GpuMemoryBuffer texture storage.
VIZ_RESOURCE_FORMAT_EXPORT bool
CanCreateGpuMemoryBufferForSinglePlaneSharedImageFormat(
    SharedImageFormat format);

// Checks if there is an equivalent BufferFormat.
VIZ_RESOURCE_FORMAT_EXPORT bool HasEquivalentBufferFormat(
    SharedImageFormat format);

// Returns the BufferFormat corresponding to `format`, which must be a
// single-planar format.
VIZ_RESOURCE_FORMAT_EXPORT gfx::BufferFormat
SinglePlaneSharedImageFormatToBufferFormat(SharedImageFormat format);

VIZ_RESOURCE_FORMAT_EXPORT SharedImageFormat
GetSharedImageFormat(gfx::BufferFormat format);

// Utilities that conceptually belong only on the service side, but are
// currently used by some clients. Usage is restricted to friended clients.
class VIZ_RESOURCE_FORMAT_EXPORT SharedImageFormatRestrictedSinglePlaneUtils {
 private:
  friend class media::VideoResourceUpdater;
  friend class gpu::SharedImageFormatRestrictedSinglePlaneUtilsAccessor;

  // The following functions use unsigned int instead of GLenum, since including
  // third_party/khronos/GLES2/gl2.h causes redefinition errors as
  // macros/functions defined in it conflict with macros/functions defined in
  // ui/gl/gl_bindings.h. See http://crbug.com/512833 for more information.
  static unsigned int ToGLDataFormat(SharedImageFormat format);
  static unsigned int ToGLDataType(SharedImageFormat format);

#if BUILDFLAG(ENABLE_VULKAN)
  static bool HasVkFormat(SharedImageFormat format);
  static VkFormat ToVkFormat(SharedImageFormat format);
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_UTILS_H_
