// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/software_output_device.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/vsync_provider.h"

namespace viz {

SoftwareOutputDevice::SoftwareOutputDevice()
    : SoftwareOutputDevice(base::SequencedTaskRunnerHandle::Get()) {}

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
  surface_ = SkSurface::MakeRaster(info);
}

SkCanvas* SoftwareOutputDevice::BeginPaint(const gfx::Rect& damage_rect) {
  damage_rect_ = damage_rect;
  return surface_ ? surface_->getCanvas() : nullptr;
}

void SoftwareOutputDevice::EndPaint() {}

gfx::VSyncProvider* SoftwareOutputDevice::GetVSyncProvider() {
  return vsync_provider_.get();
}

void SoftwareOutputDevice::OnSwapBuffers(
    SwapBuffersCallback swap_ack_callback) {
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(swap_ack_callback),
                                                   viewport_pixel_size_));
}

int SoftwareOutputDevice::MaxFramesPending() const {
  return 1;
}

}  // namespace viz
