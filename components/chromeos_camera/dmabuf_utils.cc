// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/dmabuf_utils.h"

#include <unistd.h>

#include <utility>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/buffer_validation.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos_camera {

bool VerifyMjpegBufferHandle(const gfx::GpuMemoryBufferHandle& gmb_handle) {
  if (gmb_handle.native_pixmap_handle.planes[0].offset != 0u) {
    DLOG(ERROR) << "Invalid DMA buf plane offset";
    return false;
  }
  // For MJPEG, we expect the byte size to be at least as large as the stride
  // (see b/142105578).
  if (base::strict_cast<uint64_t>(
          gmb_handle.native_pixmap_handle.planes[0].stride) >
      gmb_handle.native_pixmap_handle.planes[0].size) {
    DLOG(ERROR) << "Invalid DMA buf plane stride or size";
    return false;
  }
  const auto dma_buf_fd = gmb_handle.native_pixmap_handle.planes[0].fd.get();
  const off_t data_size = lseek(dma_buf_fd, /*offset=*/0, SEEK_END);
  if (data_size == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to get the size of the dma-buf";
    return false;
  }
  if (lseek(dma_buf_fd, /*offset=*/0, SEEK_SET) == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to reset the file offset of the dma-buf";
    return false;
  }
  if (!base::IsValueInRangeForNumericType<uint64_t>(data_size) ||
      base::checked_cast<uint64_t>(data_size) <
          gmb_handle.native_pixmap_handle.planes[0].size) {
    DLOG(ERROR) << "Invalid DMA buf plane size";
    return false;
  }
  return true;
}

scoped_refptr<media::VideoFrame> ConstructVideoFrame(
    std::vector<mojom::DmaBufPlanePtr> dma_buf_planes,
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    uint64_t modifier) {
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

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
  gmb_handle.native_pixmap_handle.planes.resize(num_planes);
  gmb_handle.native_pixmap_handle.modifier = modifier;
  for (size_t i = 0; i < num_planes; ++i) {
    mojo::PlatformHandle handle =
        mojo::UnwrapPlatformHandle(std::move(dma_buf_planes[i]->fd_handle));
    if (!handle.is_valid()) {
      DLOG(ERROR) << "Invalid DMA buf file descriptor";
      return nullptr;
    }
    if (dma_buf_planes[i]->stride <= 0) {
      DLOG(ERROR) << "Invalid DMA buf stride";
      return nullptr;
    }
    gmb_handle.native_pixmap_handle.planes[i].stride =
        base::checked_cast<uint32_t>(dma_buf_planes[i]->stride);
    gmb_handle.native_pixmap_handle.planes[i].offset =
        base::strict_cast<uint64_t>(dma_buf_planes[i]->offset);
    gmb_handle.native_pixmap_handle.planes[i].size =
        base::strict_cast<uint64_t>(dma_buf_planes[i]->size);
    gmb_handle.native_pixmap_handle.planes[i].fd = handle.TakeFD();
  }
  if (pixel_format == media::PIXEL_FORMAT_MJPEG) {
    if (!VerifyMjpegBufferHandle(gmb_handle)) {
      return nullptr;
    }
  } else {
    if (!media::VerifyGpuMemoryBufferHandle(pixel_format, coded_size,
                                            gmb_handle)) {
      return nullptr;
    }
  }

  std::vector<base::ScopedFD> dma_buf_fds(num_planes);
  std::vector<media::ColorPlaneLayout> planes(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    dma_buf_fds[i] = std::move(gmb_handle.native_pixmap_handle.planes[i].fd);
    planes[i] = media::ColorPlaneLayout(
        dma_buf_planes[i]->stride,
        base::strict_cast<size_t>(dma_buf_planes[i]->offset),
        base::strict_cast<size_t>(dma_buf_planes[i]->size));
  }
  const std::optional<media::VideoFrameLayout> layout =
      media::VideoFrameLayout::CreateWithPlanes(
          pixel_format, coded_size, std::move(planes),
          media::VideoFrameLayout::kBufferAddressAlignment, modifier);
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
