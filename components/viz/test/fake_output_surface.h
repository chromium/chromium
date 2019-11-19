// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_TEST_FAKE_OUTPUT_SURFACE_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/test/test_context_provider.h"

namespace viz {

class FakeOutputSurface : public OutputSurface {
 public:
  ~FakeOutputSurface() override;

  static std::unique_ptr<FakeOutputSurface> Create3d() {
    auto provider = TestContextProvider::Create();
    provider->BindToCurrentThread();
    return base::WrapUnique(new FakeOutputSurface(std::move(provider)));
  }

  static std::unique_ptr<FakeOutputSurface> Create3d(
      scoped_refptr<ContextProvider> context_provider) {
    return base::WrapUnique(new FakeOutputSurface(context_provider));
  }

  static std::unique_ptr<FakeOutputSurface> CreateSoftware(
      std::unique_ptr<SoftwareOutputDevice> software_device) {
    return base::WrapUnique(new FakeOutputSurface(std::move(software_device)));
  }

  static std::unique_ptr<FakeOutputSurface> CreateOffscreen(
      scoped_refptr<ContextProvider> context_provider) {
    auto surface =
        base::WrapUnique(new FakeOutputSurface(std::move(context_provider)));
    surface->capabilities_.uses_default_gl_framebuffer = false;
    return surface;
  }

  void set_max_frames_pending(int max) {
    capabilities_.max_frames_pending = max;
  }

  void set_supports_dc_layers(bool supports) {
    capabilities_.supports_dc_layers = supports;
  }

  OutputSurfaceFrame* last_sent_frame() { return last_sent_frame_.get(); }
  size_t num_sent_frames() { return num_sent_frames_; }

  OutputSurfaceClient* client() { return client_; }

  void BindToClient(OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& rect) override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override {}
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  unsigned UpdateGpuFence() override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
#endif

  void set_framebuffer(GLint framebuffer, GLenum format) {
    framebuffer_ = framebuffer;
    framebuffer_format_ = format;
  }

  void set_gpu_fence_id(unsigned gpu_fence_id) { gpu_fence_id_ = gpu_fence_id; }

  void set_overlay_texture_id(unsigned overlay_texture_id) {
    overlay_texture_id_ = overlay_texture_id;
  }

  void set_has_external_stencil_test(bool has_test) {
    has_external_stencil_test_ = has_test;
  }

  const gfx::ColorSpace& last_reshape_color_space() {
    return last_reshape_color_space_;
  }

  const gfx::Rect& last_set_draw_rectangle() {
    return last_set_draw_rectangle_;
  }

 protected:
  explicit FakeOutputSurface(scoped_refptr<ContextProvider> context_provider);
  explicit FakeOutputSurface(
      std::unique_ptr<SoftwareOutputDevice> software_device);

  OutputSurfaceClient* client_ = nullptr;
  std::unique_ptr<OutputSurfaceFrame> last_sent_frame_;
  size_t num_sent_frames_ = 0;
  bool has_external_stencil_test_ = false;
  GLint framebuffer_ = 0;
  GLenum framebuffer_format_ = 0;
  unsigned gpu_fence_id_ = 0;
  unsigned overlay_texture_id_ = 0;
  gfx::ColorSpace last_reshape_color_space_;
  gfx::Rect last_set_draw_rectangle_;

 private:
  void SwapBuffersAck();

  base::WeakPtrFactory<FakeOutputSurface> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_OUTPUT_SURFACE_H_
