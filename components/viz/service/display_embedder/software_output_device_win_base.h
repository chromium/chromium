// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_BASE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_BASE_H_

#include <windows.h>

#include "base/threading/thread_checker.h"
#include "components/viz/service/display/software_output_device.h"

namespace viz {

// Shared base class for Windows SoftwareOutputDevice implementations.
class VIZ_SERVICE_EXPORT SoftwareOutputDeviceWinBase
    : public SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceWinBase(HWND hwnd);
  SoftwareOutputDeviceWinBase(const SoftwareOutputDeviceWinBase& other) =
      delete;
  SoftwareOutputDeviceWinBase& operator=(
      const SoftwareOutputDeviceWinBase& other) = delete;
  ~SoftwareOutputDeviceWinBase() override;

  HWND hwnd() const { return hwnd_; }

  // SoftwareOutputDevice implementation.
  void Resize(const gfx::Size& viewport_pixel_size,
              float scale_factor) override;
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override;
  void EndPaint() override;

 protected:
  // Notifies the `OutputDeviceBacking` that its client has resized. This
  // function is called after `viewport_pixel_size_` has been updated since the
  // backing uses it when deciding to vacate the staging texture.
  virtual void NotifyClientResized() {}

  // Called from Resize() if |viewport_pixel_size_| has changed. Returns whether
  // the resize was successful.
  virtual bool ResizeDelegated(const gfx::Size& viewport_pixel_size) = 0;

  // Called from BeginPaint() and should return an SkCanvas.
  virtual SkCanvas* BeginPaintDelegated() = 0;

  // Called from EndPaint() if there is damage.
  virtual void EndPaintDelegated(const gfx::Rect& damage_rect) = 0;

 private:
  const HWND hwnd_;
  bool in_paint_ = false;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_BASE_H_
