// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_UTILS_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_UTILS_H_

#include "base/component_export.h"
#include "components/viz/common/resources/shared_image_format.h"

namespace gfx {
enum class BufferFormat : uint8_t;
}

enum SkColorType : int;

namespace viz {

// Returns the closest SkColorType for a given single planar `format`.
//
// NOTE: The formats BGRX_8888, BGR_565 and BGRA_1010102 return a SkColorType
// with R/G channels reversed. This is because from GPU perspective, GL format
// is always RGBA and there is no difference between RGBA/BGRA. Also, these
// formats should not be used for software SkImages/SkSurfaces.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
SkColorType ToClosestSkColorType(SharedImageFormat format);

// Returns the closest SkColorType for a given `format` that does not prefer
// external sampler and `plane_index`. For single planar formats (eg. RGBA) the
// plane_index must be zero and it's equivalent to calling function above.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
SkColorType ToClosestSkColorType(SharedImageFormat format, int plane_index);

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

// Returns the BufferFormat corresponding to `format`.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
gfx::BufferFormat SharedImageFormatToBufferFormat(SharedImageFormat format);

// Returns the SharedImageFormat corresponding to `buffer_format`.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
SharedImageFormat GetSharedImageFormat(gfx::BufferFormat buffer_format);

// Returns the shared memory offset for `plane_index` for a `format` of `size`.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
size_t SharedMemoryOffsetForSharedImageFormat(SharedImageFormat format,
                                              int plane_index,
                                              const gfx::Size& size);

// Calculates the row size in bytes for a shared memory plane of a
// SharedImageFormat. Returns size on success and std::nullopt if the row size
// exceeds the maximum value of `size_t`.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
std::optional<size_t> SharedMemoryRowSizeForSharedImageFormat(
    SharedImageFormat format,
    int plane_index,
    int width);

// Calculates the plane size in bytes for a shared memory plane of a
// SharedImageFormat. Returns true on success and false if the plane size
// exceeds the maximum value of `size_t`.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
std::optional<size_t> SharedMemoryPlaneSizeForSharedImageFormat(
    SharedImageFormat format,
    int plane_index,
    const gfx::Size& size);

// Calculates the image size in bytes for shared memory of a
// SharedImageFormat. Returns true on success and false if the image size
// exceeds the maximum value of `size_t`.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
std::optional<size_t> SharedMemorySizeForSharedImageFormat(
    SharedImageFormat format,
    const gfx::Size& size);

// Multiplanar buffer formats (e.g, YUV_420_BIPLANAR, YVU_420, P010) can be
// tricky when the size of the primary plane is odd, because the subsampled
// planes will have a size that is not a divisor of the primary plane's size.
// This returns whether odd size multiplanar formats are supported.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
bool IsOddSizeMultiPlanarBuffersAllowed();

// Returns a span containing all mappable SharedImageFormats.
COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT)
base::span<const SharedImageFormat> GetMappableSharedImageFormatForTesting();
}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_UTILS_H_
