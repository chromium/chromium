// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_ozone.h"

#include <memory>
#include <utility>

#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace viz {

SoftwareOutputDeviceOzone::SoftwareOutputDeviceOzone(
    std::unique_ptr<ui::PlatformWindowSurface> platform_window_surface,
    std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone)
    : platform_window_surface_(std::move(platform_window_surface)),
      surface_ozone_(std::move(surface_ozone)) {
  vsync_provider_ = surface_ozone_->CreateVSyncProvider();
}

SoftwareOutputDeviceOzone::~SoftwareOutputDeviceOzone() {}

void SoftwareOutputDeviceOzone::Resize(const gfx::Size& viewport_pixel_size,
                                       float scale_factor) {
  if (viewport_pixel_size_ == viewport_pixel_size)
    return;

  viewport_pixel_size_ = viewport_pixel_size;

  surface_ozone_->ResizeCanvas(viewport_pixel_size_, scale_factor);
}

SkCanvas* SoftwareOutputDeviceOzone::BeginPaint(const gfx::Rect& damage_rect) {
  DCHECK(gfx::Rect(viewport_pixel_size_).Contains(damage_rect));

  damage_rect_ = damage_rect;

  // Get canvas for next frame.
  return surface_ozone_->GetCanvas();
}

void SoftwareOutputDeviceOzone::EndPaint() {
  SoftwareOutputDevice::EndPaint();

  surface_ozone_->PresentCanvas(damage_rect_);
}

void SoftwareOutputDeviceOzone::OnSwapBuffers(
    SwapBuffersCallback swap_ack_callback,
    gfx::FrameData data) {
  if (surface_ozone_->SupportsAsyncBufferSwap())
    surface_ozone_->OnSwapBuffers(std::move(swap_ack_callback), data);
  else
    SoftwareOutputDevice::OnSwapBuffers(std::move(swap_ack_callback), data);
}

int SoftwareOutputDeviceOzone::MaxFramesPending() const {
  return surface_ozone_->MaxFramesPending();
}

bool SoftwareOutputDeviceOzone::SupportsOverridePlatformSize() const {
  return surface_ozone_->SupportsOverridePlatformSize();
}

}  // namespace viz
