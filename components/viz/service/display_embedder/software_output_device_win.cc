// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_win.h"

#include "base/bind.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"
#include "components/viz/common/display/use_layered_window.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/service/display_embedder/output_device_backing.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/viz/privileged/mojom/compositing/layered_window_updater.mojom.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/gl/vsync_provider_win.h"

namespace viz {
namespace {

// Shared base class for Windows SoftwareOutputDevice implementations.
class SoftwareOutputDeviceWinBase : public SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceWinBase(HWND hwnd) : hwnd_(hwnd) {
    vsync_provider_ = std::make_unique<gl::VSyncProviderWin>(hwnd);
  }

  ~SoftwareOutputDeviceWinBase() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(!in_paint_);
  }

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

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceWinBase);
};

void SoftwareOutputDeviceWinBase::Resize(const gfx::Size& viewport_pixel_size,
                                         float scale_factor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!in_paint_);

  if (viewport_pixel_size_ == viewport_pixel_size)
    return;

  viewport_pixel_size_ = viewport_pixel_size;
  ResizeDelegated();
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
  if (intersected_damage_rect.IsEmpty())
    return;

  EndPaintDelegated(intersected_damage_rect);
}

// SoftwareOutputDevice implementation that draws directly to the provided HWND.
// The backing buffer for paint is shared for all instances of this class.
class SoftwareOutputDeviceWinDirect : public SoftwareOutputDeviceWinBase,
                                      public OutputDeviceBacking::Client {
 public:
  SoftwareOutputDeviceWinDirect(HWND hwnd, OutputDeviceBacking* backing);
  ~SoftwareOutputDeviceWinDirect() override;

  // SoftwareOutputDeviceWinBase implementation.
  void ResizeDelegated() override;
  SkCanvas* BeginPaintDelegated() override;
  void EndPaintDelegated(const gfx::Rect& damage_rect) override;

  // OutputDeviceBacking::Client implementation.
  const gfx::Size& GetViewportPixelSize() const override {
    return viewport_pixel_size_;
  }
  void ReleaseCanvas() override { canvas_.reset(); }

 private:
  OutputDeviceBacking* const backing_;
  std::unique_ptr<SkCanvas> canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceWinDirect);
};

SoftwareOutputDeviceWinDirect::SoftwareOutputDeviceWinDirect(
    HWND hwnd,
    OutputDeviceBacking* backing)
    : SoftwareOutputDeviceWinBase(hwnd), backing_(backing) {
  backing_->RegisterClient(this);
}

SoftwareOutputDeviceWinDirect::~SoftwareOutputDeviceWinDirect() {
  backing_->UnregisterClient(this);
}

void SoftwareOutputDeviceWinDirect::ResizeDelegated() {
  canvas_.reset();
  backing_->ClientResized();
}

SkCanvas* SoftwareOutputDeviceWinDirect::BeginPaintDelegated() {
  if (!canvas_) {
    // Share pixel backing with other SoftwareOutputDeviceWinDirect instances.
    // All work happens on the same thread so this is safe.
    base::UnsafeSharedMemoryRegion* region =
        backing_->GetSharedMemoryRegion(viewport_pixel_size_);
    if (region && region->IsValid()) {
      canvas_ = skia::CreatePlatformCanvasWithSharedSection(
          viewport_pixel_size_.width(), viewport_pixel_size_.height(), true,
          region->GetPlatformHandle(), skia::CRASH_ON_FAILURE);
    }
  }
  return canvas_.get();
}

void SoftwareOutputDeviceWinDirect::EndPaintDelegated(
    const gfx::Rect& damage_rect) {
  if (!canvas_)
    return;

  HDC dib_dc = skia::GetNativeDrawingContext(canvas_.get());
  HDC hdc = ::GetDC(hwnd());
  RECT src_rect = damage_rect.ToRECT();
  skia::CopyHDC(dib_dc, hdc, damage_rect.x(), damage_rect.y(),
                canvas_->imageInfo().isOpaque(), src_rect,
                canvas_->getTotalMatrix());

  ::ReleaseDC(hwnd(), hdc);
}

// SoftwareOutputDevice implementation that uses layered window API to draw
// indirectly. Since UpdateLayeredWindow() is blocked by the GPU sandbox an
// implementation of mojom::LayeredWindowUpdater in the browser process handles
// calling UpdateLayeredWindow. Pixel backing is in a SharedMemoryRegion so no
// copying between processes is required.
class SoftwareOutputDeviceWinProxy : public SoftwareOutputDeviceWinBase {
 public:
  SoftwareOutputDeviceWinProxy(
      HWND hwnd,
      mojo::PendingRemote<mojom::LayeredWindowUpdater> layered_window_updater);
  ~SoftwareOutputDeviceWinProxy() override = default;

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

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceWinProxy);
};

SoftwareOutputDeviceWinProxy::SoftwareOutputDeviceWinProxy(
    HWND hwnd,
    mojo::PendingRemote<mojom::LayeredWindowUpdater> layered_window_updater)
    : SoftwareOutputDeviceWinBase(hwnd),
      layered_window_updater_(std::move(layered_window_updater)) {
  DCHECK(layered_window_updater_.is_bound());
}

void SoftwareOutputDeviceWinProxy::OnSwapBuffers(
    SwapBuffersCallback swap_ack_callback) {
  DCHECK(swap_ack_callback_.is_null());

  // We aren't waiting on DrawAck() and can immediately run the callback.
  if (!waiting_on_draw_ack_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(swap_ack_callback), viewport_pixel_size_));
    return;
  }

  swap_ack_callback_ =
      base::BindOnce(std::move(swap_ack_callback), viewport_pixel_size_);
}

void SoftwareOutputDeviceWinProxy::ResizeDelegated() {
  canvas_.reset();

  size_t required_bytes;
  if (!ResourceSizes::MaybeSizeInBytes(
          viewport_pixel_size_, ResourceFormat::RGBA_8888, &required_bytes)) {
    DLOG(ERROR) << "Invalid viewport size " << viewport_pixel_size_.ToString();
    return;
  }

  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(required_bytes);
  if (!region.IsValid()) {
    DLOG(ERROR) << "Failed to allocate " << required_bytes << " bytes";
    return;
  }

  // The SkCanvas maps shared memory on creation and unmaps on destruction.
  canvas_ = skia::CreatePlatformCanvasWithSharedSection(
      viewport_pixel_size_.width(), viewport_pixel_size_.height(), true,
      region.GetPlatformHandle(), skia::CRASH_ON_FAILURE);

  // Transfer region ownership to the browser process.
  layered_window_updater_->OnAllocatedSharedMemory(viewport_pixel_size_,
                                                   std::move(region));
}

SkCanvas* SoftwareOutputDeviceWinProxy::BeginPaintDelegated() {
  return canvas_.get();
}

void SoftwareOutputDeviceWinProxy::EndPaintDelegated(
    const gfx::Rect& damage_rect) {
  DCHECK(!waiting_on_draw_ack_);

  if (!canvas_)
    return;

  layered_window_updater_->Draw(base::BindOnce(
      &SoftwareOutputDeviceWinProxy::DrawAck, base::Unretained(this)));
  waiting_on_draw_ack_ = true;

  TRACE_EVENT_ASYNC_BEGIN0("viz", "SoftwareOutputDeviceWinProxy::Draw", this);
}

void SoftwareOutputDeviceWinProxy::DrawAck() {
  DCHECK(waiting_on_draw_ack_);
  DCHECK(!swap_ack_callback_.is_null());

  TRACE_EVENT_ASYNC_END0("viz", "SoftwareOutputDeviceWinProxy::Draw", this);

  waiting_on_draw_ack_ = false;
  std::move(swap_ack_callback_).Run();
}

}  // namespace

std::unique_ptr<SoftwareOutputDevice> CreateSoftwareOutputDeviceWin(
    HWND hwnd,
    OutputDeviceBacking* backing,
    mojom::DisplayClient* display_client) {
  if (NeedsToUseLayerWindow(hwnd)) {
    DCHECK(display_client);

    // Setup mojom::LayeredWindowUpdater implementation in the browser process
    // to draw to the HWND.
    mojo::PendingRemote<mojom::LayeredWindowUpdater> layered_window_updater;
    display_client->CreateLayeredWindowUpdater(
        layered_window_updater.InitWithNewPipeAndPassReceiver());

    return std::make_unique<SoftwareOutputDeviceWinProxy>(
        hwnd, std::move(layered_window_updater));
  } else {
    return std::make_unique<SoftwareOutputDeviceWinDirect>(hwnd, backing);
  }
}

}  // namespace viz
