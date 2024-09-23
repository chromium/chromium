// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_UTILS_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_UTILS_H_

#include "base/component_export.h"
#include "components/viz/common/resources/shared_image_format.h"

namespace gpu {
class ClientSharedImage;
class SharedImageFormatToBufferFormatRestrictedUtilsAccessor;
class SharedImageFormatRestrictedUtilsAccessor;
class TestSharedImageInterface;
}  // namespace gpu

namespace cc {
class PerfContextProvider;
}

namespace gfx {
enum class BufferFormat : uint8_t;
}

namespace media {
class VideoFrame;
}

enum SkColorType : int;

namespace viz {

class ContextProviderCommandBuffer;
class TestContextProvider;
class TestInProcessContextProvider;

// Returns the closest SkColorType for a given single planar `format`.
//
// NOTE: The formats BGRX_8888, BGR_565 and BGRA_1010102 return a SkColorType
// with R/G channels reversed. This is because from GPU perspective, GL format
// is always RGBA and there is no difference between RGBA/BGRA. Also, these
// formats should not be used for software SkImages/SkSurfaces.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
SkColorType ToClosestSkColorType(bool gpu_compositing,
                                 SharedImageFormat format);

// Returns the closest SkColorType for a given `format` that does not prefer
// external sampler and `plane_index`. For single planar formats (eg. RGBA) the
// plane_index must be zero and it's equivalent to calling function above.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
SkColorType ToClosestSkColorType(bool gpu_compositing,
                                 SharedImageFormat format,
                                 int plane_index);

// Returns the single-plane SharedImageFormat corresponding to `color_type.`
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
SharedImageFormat SkColorTypeToSinglePlaneSharedImageFormat(
    SkColorType color_type);

// Returns whether `format`, which must be a single-planar format, can be used
// with GpuMemoryBuffer texture storage.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
bool CanCreateGpuMemoryBufferForSinglePlaneSharedImageFormat(
    SharedImageFormat format);

// Checks if there is an equivalent BufferFormat.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
bool HasEquivalentBufferFormat(SharedImageFormat format);

// Returns the BufferFormat corresponding to `format`, which must be a
// single-planar format.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
gfx::BufferFormat SinglePlaneSharedImageFormatToBufferFormat(
    SharedImageFormat format);

// Returns the SharedImageFormat corresponding to `buffer_format`.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
SharedImageFormat GetSharedImageFormat(gfx::BufferFormat buffer_format);

// Utilities that conceptually belong only on the service side, but are
// currently used by some clients. Usage is restricted to friended clients.
class COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
    SharedImageFormatRestrictedSinglePlaneUtils {
 private:
  friend class ContextProviderCommandBuffer;
  friend class TestContextProvider;
  friend class TestInProcessContextProvider;
  friend class cc::PerfContextProvider;
  friend class gpu::SharedImageFormatRestrictedUtilsAccessor;

  // |use_angle_rgbx_format| should be true when the
  // GL_ANGLE_rgbx_internal_format extension is available.
  static unsigned int ToGLTextureStorageFormat(SharedImageFormat format,
                                               bool use_angle_rgbx_format);
};

// Utility function which conceptually belong only on the service side, but are
// currently used by some clients. Usage is restricted to friended class.
class COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
    SharedImageFormatToBufferFormatRestrictedUtils {
 private:
  friend class gpu::ClientSharedImage;
  friend class gpu::SharedImageFormatToBufferFormatRestrictedUtilsAccessor;
  friend class gpu::TestSharedImageInterface;
  friend class media::VideoFrame;

  // BufferFormat is being transitioned out of SharedImage code (to use
  // SharedImageFormat instead). Refrain from using this function or preferably
  // use with single planar SharedImageFormats. Returns BufferFormat for given
  // `format`.
  static gfx::BufferFormat ToBufferFormat(SharedImageFormat format);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_UTILS_H_
