// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_x11.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/xproto.h"

namespace viz {

namespace {

x11::GraphicsContext CreateGC(x11::Connection* connection, x11::Window window) {
  auto gc = connection->GenerateId<x11::GraphicsContext>();
  connection->CreateGC({gc, window});
  return gc;
}

}  // namespace

SkiaOutputDeviceX11::SkiaOutputDeviceX11(
    scoped_refptr<gpu::SharedContextState> context_state,
    gfx::AcceleratedWidget widget,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDeviceOffscreen(context_state,
                                gfx::SurfaceOrigin::kTopLeft,
                                true /* has_alpha */,
                                memory_tracker,
                                did_swap_buffer_complete_callback),
      connection_(x11::Connection::Get()),
      window_(static_cast<x11::Window>(widget)),
      gc_(CreateGC(connection_, window_)) {
  if (auto attributes = connection_->GetWindowAttributes({window_}).Sync()) {
    visual_ = attributes->visual;
  } else {
    LOG(FATAL) << "Failed to get attributes for window "
               << static_cast<uint32_t>(window_);
  }

  // |capabilities_| should be set by SkiaOutputDeviceOffscreen.
  DCHECK_EQ(capabilities_.output_surface_origin, gfx::SurfaceOrigin::kTopLeft);
  DCHECK(capabilities_.supports_post_sub_buffer);
}

SkiaOutputDeviceX11::~SkiaOutputDeviceX11() {
  connection_->FreeGC({gc_});
}

bool SkiaOutputDeviceX11::Reshape(const gfx::Size& size,
                                  float device_scale_factor,
                                  const gfx::ColorSpace& color_space,
                                  gfx::BufferFormat format,
                                  gfx::OverlayTransform transform) {
  if (!SkiaOutputDeviceOffscreen::Reshape(size, device_scale_factor,
                                          color_space, format, transform)) {
    return false;
  }
  auto ii =
      SkImageInfo::MakeN32(size.width(), size.height(), kOpaque_SkAlphaType);
  std::vector<uint8_t> mem(ii.computeMinByteSize());
  pixels_ = base::RefCountedBytes::TakeVector(&mem);
  return true;
}

void SkiaOutputDeviceX11::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  return PostSubBuffer(
      gfx::Rect(0, 0, sk_surface_->width(), sk_surface_->height()),
      std::move(feedback), std::move(latency_info));
}

void SkiaOutputDeviceX11::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers(std::move(feedback));
  if (!rect.IsEmpty()) {
    auto ii =
        SkImageInfo::MakeN32(rect.width(), rect.height(), kOpaque_SkAlphaType);
    DCHECK_GE(pixels_->size(), ii.computeMinByteSize());
    SkPixmap sk_pixmap(ii, pixels_->data(), ii.minRowBytes());
    bool result = sk_surface_->readPixels(sk_pixmap, rect.x(), rect.y());
    LOG_IF(FATAL, !result) << "Failed to read pixels from offscreen SkSurface.";

    // TODO(penghuang): Switch to XShmPutImage.
    ui::DrawPixmap(x11::Connection::Get(), visual_, window_, gc_, sk_pixmap,
                   0 /* src_x */, 0 /* src_y */, rect.x() /* dst_x */,
                   rect.y() /* dst_y */, rect.width(), rect.height());

    connection_->Flush();
  }
  FinishSwapBuffers(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK),
                    gfx::Size(sk_surface_->width(), sk_surface_->height()),
                    std::move(latency_info));
}

}  // namespace viz
