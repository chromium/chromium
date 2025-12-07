// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_OUTPUT_DEVICE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_OUTPUT_DEVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/viz/service/display/software_output_device_client.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

class SkCanvas;

namespace gfx {
class VSyncProvider;
}  // namespace gfx

namespace viz {

class SoftwareOutputDeviceClient;

// This is a "tear-off" class providing software drawing support to
// OutputSurface, such as to a platform-provided window framebuffer.
class VIZ_SERVICE_EXPORT SoftwareOutputDevice {
 public:
  // Uses TaskRunner returned from SequencedTaskRunner::GetCurrentDefault().
  SoftwareOutputDevice();
  explicit SoftwareOutputDevice(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  SoftwareOutputDevice(const SoftwareOutputDevice&) = delete;
  SoftwareOutputDevice& operator=(const SoftwareOutputDevice&) = delete;

  virtual ~SoftwareOutputDevice();

  // This may be called only once, and requires a non-nullptr argument.
  void BindToClient(SoftwareOutputDeviceClient* client);

  // Discards any pre-existing backing buffers and allocates memory for a
  // software device of |size|. This must be called before the
  // |SoftwareOutputDevice| can be used in other ways.
  virtual void Resize(const gfx::Size& pixel_size, float scale_factor);

  // Called on BeginDrawingFrame. The compositor will draw into the returned
  // SkCanvas. The |SoftwareOutputDevice| implementation needs to provide a
  // valid SkCanvas of at least size |damage_rect|. This class retains ownership
  // of the SkCanvas.
  virtual SkCanvas* BeginPaint(const gfx::Rect& damage_rect);

  // Called on FinishDrawingFrame. The compositor will no longer mutate the the
  // SkCanvas instance returned by |BeginPaint| and should discard any reference
  // that it holds to it.
  virtual void EndPaint();

  // Discard the backing buffer in the surface provided by this instance.
  virtual void DiscardBackbuffer() {}

  // Ensures that there is a backing buffer available on this instance.
  virtual void EnsureBackbuffer() {}

  // VSyncProvider used to update the timer used to schedule draws with the
  // hardware vsync. Return null if a provider doesn't exist.
  virtual gfx::VSyncProvider* GetVSyncProvider();

  using SwapBuffersCallback = base::OnceCallback<void(const gfx::Size&)>;
  // Called from OutputSurface::SwapBuffers(). The default implementation will
  // immediately run |swap_ack_callback| via PostTask. If swap isn't synchronous
  // this can be overriden so that |swap_ack_callback| is run after swap
  // completes.
  virtual void OnSwapBuffers(SwapBuffersCallback swap_ack_callback,
                             gfx::FrameData data);

  virtual int MaxFramesPending() const;

  // Returns true if we are allowed to adopt a size different from the
  // platform's proposed surface size.
  virtual bool SupportsOverridePlatformSize() const;

  // Copy and return the contents of |surface_|.
  SkBitmap ReadbackForTesting();

 protected:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<SoftwareOutputDeviceClient> client_ = nullptr;
  gfx::Size viewport_pixel_size_;
  gfx::Rect damage_rect_;
  sk_sp<SkSurface> surface_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_OUTPUT_DEVICE_H_
