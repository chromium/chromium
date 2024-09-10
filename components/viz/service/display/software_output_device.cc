// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/software_output_device.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/vsync_provider.h"

namespace viz {

SoftwareOutputDevice::SoftwareOutputDevice()
    : SoftwareOutputDevice(base::SequencedTaskRunner::GetCurrentDefault()) {}

SoftwareOutputDevice::SoftwareOutputDevice(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

SoftwareOutputDevice::~SoftwareOutputDevice() = default;

void SoftwareOutputDevice::BindToClient(SoftwareOutputDeviceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void SoftwareOutputDevice::Resize(const gfx::Size& viewport_pixel_size,
                                  float scale_factor) {
  if (viewport_pixel_size_ == viewport_pixel_size)
    return;

  SkImageInfo info =
      SkImageInfo::MakeN32(viewport_pixel_size.width(),
                           viewport_pixel_size.height(), kOpaque_SkAlphaType);
  viewport_pixel_size_ = viewport_pixel_size;
  SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
  surface_ = SkSurfaces::Raster(info, &props);
}

SkCanvas* SoftwareOutputDevice::BeginPaint(const gfx::Rect& damage_rect) {
  damage_rect_ = damage_rect;
  return surface_ ? surface_->getCanvas() : nullptr;
}

void SoftwareOutputDevice::EndPaint() {}

gfx::VSyncProvider* SoftwareOutputDevice::GetVSyncProvider() {
  return vsync_provider_.get();
}

void SoftwareOutputDevice::OnSwapBuffers(SwapBuffersCallback swap_ack_callback,
                                         gfx::FrameData data) {
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(swap_ack_callback),
                                                   viewport_pixel_size_));
}

int SoftwareOutputDevice::MaxFramesPending() const {
  return 1;
}

bool SoftwareOutputDevice::SupportsOverridePlatformSize() const {
  return false;
}

SkBitmap SoftwareOutputDevice::ReadbackForTesting() {
  SkBitmap bitmap;
  bitmap.allocPixels(
      surface_->imageInfo().makeColorSpace(SkColorSpace::MakeSRGB()));
  CHECK(surface_->readPixels(bitmap, 0, 0));
  return bitmap;
}

}  // namespace viz
