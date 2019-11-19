// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_x11.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/base/x/x11_shm_image_pool.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"

namespace viz {

SoftwareOutputDeviceX11::SoftwareOutputDeviceX11(
    gfx::AcceleratedWidget widget,
    base::TaskRunner* gpu_task_runner)
    : x11_software_bitmap_presenter_(widget,
                                     task_runner_.get(),
                                     gpu_task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

SoftwareOutputDeviceX11::~SoftwareOutputDeviceX11() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SoftwareOutputDeviceX11::Resize(const gfx::Size& pixel_size,
                                     float scale_factor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  viewport_pixel_size_ = pixel_size;
  x11_software_bitmap_presenter_.Resize(pixel_size);
}

SkCanvas* SoftwareOutputDeviceX11::BeginPaint(const gfx::Rect& damage_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  damage_rect_ = damage_rect;
  return x11_software_bitmap_presenter_.GetSkCanvas();
}

void SoftwareOutputDeviceX11::EndPaint() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SoftwareOutputDevice::EndPaint();
  x11_software_bitmap_presenter_.EndPaint(damage_rect_);
}

void SoftwareOutputDeviceX11::OnSwapBuffers(
    SwapBuffersCallback swap_ack_callback) {
  x11_software_bitmap_presenter_.OnSwapBuffers(std::move(swap_ack_callback));
}

int SoftwareOutputDeviceX11::MaxFramesPending() const {
  return x11_software_bitmap_presenter_.MaxFramesPending();
}

}  // namespace viz
