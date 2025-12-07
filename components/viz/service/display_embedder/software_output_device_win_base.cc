// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_win_base.h"

#include <memory>
#include <utility>

#include "ui/gl/vsync_provider_win.h"

namespace viz {

SoftwareOutputDeviceWinBase::SoftwareOutputDeviceWinBase(HWND hwnd)
    : hwnd_(hwnd) {
  vsync_provider_ = std::make_unique<gl::VSyncProviderWin>(hwnd);
}

SoftwareOutputDeviceWinBase::~SoftwareOutputDeviceWinBase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!in_paint_);
}

void SoftwareOutputDeviceWinBase::Resize(const gfx::Size& viewport_pixel_size,
                                         float scale_factor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!in_paint_);

  if (viewport_pixel_size_ == viewport_pixel_size) {
    return;
  }

  if (ResizeDelegated(viewport_pixel_size)) {
    viewport_pixel_size_ = viewport_pixel_size;
    NotifyClientResized();
  }
}

SkCanvas* SoftwareOutputDeviceWinBase::BeginPaint(
    const gfx::Rect& damage_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!in_paint_);

  damage_rect_ = damage_rect;
  in_paint_ = true;
  return BeginPaintDelegated();
}

void SoftwareOutputDeviceWinBase::EndPaint() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(in_paint_);

  in_paint_ = false;

  gfx::Rect intersected_damage_rect = damage_rect_;
  intersected_damage_rect.Intersect(gfx::Rect(viewport_pixel_size_));
  if (intersected_damage_rect.IsEmpty()) {
    return;
  }

  EndPaintDelegated(intersected_damage_rect);
}

}  // namespace viz
