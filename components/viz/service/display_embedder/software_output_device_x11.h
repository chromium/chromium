// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_X11_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_X11_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/base/x/x11_software_bitmap_presenter.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

namespace viz {

class VIZ_SERVICE_EXPORT SoftwareOutputDeviceX11 : public SoftwareOutputDevice {
 public:
  SoftwareOutputDeviceX11(gfx::AcceleratedWidget widget,
                          base::TaskRunner* gpu_task_runner);

  ~SoftwareOutputDeviceX11() override;

 private:
  // SoftwareOutputDevice overrides:
  void Resize(const gfx::Size& pixel_size, float scale_factor) override;
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override;
  void EndPaint() override;
  void OnSwapBuffers(SwapBuffersCallback swap_ack_callback) override;
  int MaxFramesPending() const override;

  ui::X11SoftwareBitmapPresenter x11_software_bitmap_presenter_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceX11);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_X11_H_
