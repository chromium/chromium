// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_H_

#include <windows.h>

#include <memory>

#include "base/threading/thread_checker.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display_embedder/output_device_backing.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/layered_window_updater.mojom.h"
#include "third_party/skia/include/core/SkCanvas.h"

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

  // Called from Resize() if |viewport_pixel_size_| has changed.
  virtual void ResizeDelegated() = 0;

  // Called from BeginPaint() and should return an SkCanvas.
  virtual SkCanvas* BeginPaintDelegated() = 0;

  // Called from EndPaint() if there is damage.
  virtual void EndPaintDelegated(const gfx::Rect& damage_rect) = 0;

 private:
  const HWND hwnd_;
  bool in_paint_ = false;

  THREAD_CHECKER(thread_checker_);
};

// SoftwareOutputDevice implementation that draws directly to the provided HWND.
// The backing buffer for paint is shared for all instances of this class.
class VIZ_SERVICE_EXPORT SoftwareOutputDeviceWinDirect
    : public SoftwareOutputDeviceWinBase,
      public OutputDeviceBacking::Client {
 public:
  SoftwareOutputDeviceWinDirect(HWND hwnd, OutputDeviceBacking* backing);
  ~SoftwareOutputDeviceWinDirect() override;

  // SoftwareOutputDeviceWinBase implementation.
  void ResizeDelegated() override;
  SkCanvas* BeginPaintDelegated() override;
  void EndPaintDelegated(const gfx::Rect& damage_rect) override;

  // OutputDeviceBacking::Client implementation.
  const gfx::Size& GetViewportPixelSize() const override;
  void ReleaseCanvas() override;

 private:
  OutputDeviceBacking* const backing_;
  std::unique_ptr<SkCanvas> canvas_;
};

// SoftwareOutputDevice implementation that uses layered window API to draw
// indirectly. Since UpdateLayeredWindow() is blocked by the GPU sandbox an
// implementation of mojom::LayeredWindowUpdater in the browser process handles
// calling UpdateLayeredWindow. Pixel backing is in a SharedMemoryRegion so no
// copying between processes is required.
class VIZ_SERVICE_EXPORT SoftwareOutputDeviceWinProxy
    : public SoftwareOutputDeviceWinBase {
 public:
  SoftwareOutputDeviceWinProxy(
      HWND hwnd,
      mojo::PendingRemote<mojom::LayeredWindowUpdater> layered_window_updater);
  ~SoftwareOutputDeviceWinProxy() override;

  // SoftwareOutputDevice implementation.
  void OnSwapBuffers(SwapBuffersCallback swap_ack_callback) override;

  // SoftwareOutputDeviceWinBase implementation.
  void ResizeDelegated() override;
  SkCanvas* BeginPaintDelegated() override;
  void EndPaintDelegated(const gfx::Rect& rect) override;

 private:
  // Runs |swap_ack_callback_| after draw has happened.
  void DrawAck();

  mojo::Remote<mojom::LayeredWindowUpdater> layered_window_updater_;

  std::unique_ptr<SkCanvas> canvas_;
  bool waiting_on_draw_ack_ = false;
  base::OnceClosure swap_ack_callback_;
};

// Creates an appropriate SoftwareOutputDevice implementation.
VIZ_SERVICE_EXPORT std::unique_ptr<SoftwareOutputDevice>
CreateSoftwareOutputDeviceWin(HWND hwnd,
                              OutputDeviceBacking* backing,
                              mojom::DisplayClient* display_client);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_H_
