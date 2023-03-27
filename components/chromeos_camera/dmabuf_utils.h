// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_DMABUF_UTILS_H_
#define COMPONENTS_CHROMEOS_CAMERA_DMABUF_UTILS_H_

#include <stdint.h>

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/chromeos_camera/common/dmabuf.mojom.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class VideoFrame;
}

namespace chromeos_camera {

scoped_refptr<media::VideoFrame> ConstructVideoFrame(
    std::vector<mojom::DmaBufPlanePtr> dma_buf_planes,
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    uint64_t modifier = gfx::NativePixmapHandle::kNoModifier);

}  // namespace chromeos_camera

#endif  // COMPONENTS_CHROMEOS_CAMERA_DMABUF_UTILS_H_
