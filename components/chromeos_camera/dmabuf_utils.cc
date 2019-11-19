// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/dmabuf_utils.h"

#include <utility>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos_camera {

scoped_refptr<media::VideoFrame> ConstructVideoFrame(
    std::vector<mojom::DmaBufPlanePtr> dma_buf_planes,
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size) {
  const size_t num_planes = media::VideoFrame::NumPlanes(pixel_format);
  if (num_planes != dma_buf_planes.size()) {
    DLOG(ERROR) << "The number of DMA buf planes does not match the format";
    return nullptr;
  }
  if (coded_size.IsEmpty()) {
    DLOG(ERROR) << "Invalid coded size: " << coded_size.width() << ", "
                << coded_size.height();
    return nullptr;
  }
  const gfx::Rect visible_rect(coded_size);

  std::vector<base::ScopedFD> dma_buf_fds(num_planes);
  std::vector<media::ColorPlaneLayout> planes(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    mojo::PlatformHandle handle =
        mojo::UnwrapPlatformHandle(std::move(dma_buf_planes[i]->fd_handle));
    if (!handle.is_valid()) {
      DLOG(ERROR) << "Invalid DMA buf file descriptor";
      return nullptr;
    }
    dma_buf_fds[i] = handle.TakeFD();
    planes[i] = media::ColorPlaneLayout(
        dma_buf_planes[i]->stride,
        base::strict_cast<size_t>(dma_buf_planes[i]->offset),
        base::strict_cast<size_t>(dma_buf_planes[i]->size));
  }
  const base::Optional<media::VideoFrameLayout> layout =
      media::VideoFrameLayout::CreateWithPlanes(pixel_format, coded_size,
                                                std::move(planes));
  if (!layout) {
    DLOG(ERROR) << "Failed to create video frame layout";
    return nullptr;
  }

  return media::VideoFrame::WrapExternalDmabufs(
      *layout,                 // layout
      visible_rect,            // visible_rect
      coded_size,              // natural_size
      std::move(dma_buf_fds),  // dmabuf_fds
      base::TimeDelta());      // timestamp
}

}  // namespace chromeos_camera
