// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_TEST_FAKE_OUTPUT_SURFACE_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/test/test_context_provider.h"
#include "ui/gfx/overlay_transform.h"

namespace viz {

class FakeSoftwareOutputSurface : public OutputSurface {
 public:
  explicit FakeSoftwareOutputSurface(
      std::unique_ptr<SoftwareOutputDevice> software_device);
  ~FakeSoftwareOutputSurface() override;

  OutputSurfaceFrame* last_sent_frame() { return last_sent_frame_.get(); }
  size_t num_sent_frames() { return num_sent_frames_; }

  // OutputSurface implementation.
  void BindToClient(OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void Reshape(const ReshapeParams& params) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  gfx::OverlayTransform GetDisplayTransform() override;
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
#endif

  const gfx::ColorSpace& last_reshape_color_space() {
    return last_reshape_color_space_;
  }

  void set_support_display_transform_hint(bool support) {
    support_display_transform_hint_ = support;
  }

 protected:
  raw_ptr<OutputSurfaceClient> client_ = nullptr;
  std::unique_ptr<OutputSurfaceFrame> last_sent_frame_;
  size_t num_sent_frames_ = 0;
  gfx::ColorSpace last_reshape_color_space_;

  bool support_display_transform_hint_ = false;
  gfx::OverlayTransform display_transform_hint_ = gfx::OVERLAY_TRANSFORM_NONE;

 private:
  void SwapBuffersAck();

  base::WeakPtrFactory<FakeSoftwareOutputSurface> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_OUTPUT_SURFACE_H_
