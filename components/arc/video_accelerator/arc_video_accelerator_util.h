// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ACCELERATOR_ARC_VIDEO_ACCELERATOR_UTIL_H_
#define COMPONENTS_ARC_VIDEO_ACCELERATOR_ARC_VIDEO_ACCELERATOR_UTIL_H_

#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/optional.h"
#include "components/arc/video_accelerator/video_frame_plane.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/system/handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace arc {

// Creates ScopedFD from given mojo::ScopedHandle.
// Returns invalid ScopedFD on failure.
base::ScopedFD UnwrapFdFromMojoHandle(mojo::ScopedHandle handle);

// Return a list of duplicated |fd|. The size of list is |num_fds|. Return an
// empty list if duplicatation fails.
std::vector<base::ScopedFD> DuplicateFD(base::ScopedFD fd, size_t num_fds);

// Return GpuMemoryBufferHandle iff |planes| are valid for a video frame located
// on |scoped_fds| and of |pixel_format| and |coded_size|. Otherwise
// returns base::nullopt.
base::Optional<gfx::GpuMemoryBufferHandle> CreateGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    std::vector<base::ScopedFD> scoped_fds,
    const std::vector<VideoFramePlane>& planes);
base::Optional<gfx::GpuMemoryBufferHandle> CreateGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    std::vector<base::ScopedFD> scoped_fds,
    const std::vector<media::ColorPlaneLayout>& planes);

// Create a temp file and write |data| into the file.
base::ScopedFD CreateTempFileForTesting(const std::string& data);

}  // namespace arc
#endif  // COMPONENTS_ARC_VIDEO_ACCELERATOR_ARC_VIDEO_ACCELERATOR_UTIL_H_
