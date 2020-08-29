// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_H_

#include <memory>
#include <vector>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/display/update_vsync_parameters_callback.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/gpu_vsync_callback.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/gpu_task_scheduler_helper.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/surface_origin.h"
#include "ui/latency/latency_info.h"

namespace gfx {
class ColorSpace;
class Rect;
class Size;
struct SwapResponse;
}  // namespace gfx

namespace viz {
class OutputSurfaceClient;
class OutputSurfaceFrame;
class SkiaOutputSurface;

// This class represents a platform-independent API for presenting
// buffers to display via GPU or software compositing. Implementations
// can provide platform-specific behaviour.
class VIZ_SERVICE_EXPORT OutputSurface {
 public:
  enum Type {
    kSoftware = 0,
    kOpenGL = 1,
    kVulkan = 2,
  };

  enum class OrientationMode {
    kLogic,     // The orientation same to logical screen orientation as seen by
                // the user.
    kHardware,  // The orientation same to the hardware.
  };
  struct Capabilities {
    Capabilities();
    Capabilities(const Capabilities& capabilities);

    int max_frames_pending = 1;
    // The number of buffers for the SkiaOutputDevice. If the
    // |supports_post_sub_buffer| true, SkiaOutputSurfaceImpl will track target
    // damaged area based on this number.
    int number_of_buffers = 2;
    // Whether this output surface renders to the default OpenGL zero
    // framebuffer or to an offscreen framebuffer.
    bool uses_default_gl_framebuffer = true;
    // Where (0,0) is on this OutputSurface.
    gfx::SurfaceOrigin output_surface_origin = gfx::SurfaceOrigin::kBottomLeft;
    // Whether this OutputSurface supports stencil operations or not.
    // Note: HasExternalStencilTest() must return false when an output surface
    // has been configured for stencil usage.
    bool supports_stencil = false;
    // Whether this OutputSurface supports post sub buffer or not.
    bool supports_post_sub_buffer = false;
    // Whether this OutputSurface supports commit overlay planes.
    bool supports_commit_overlay_planes = false;
    // Whether this OutputSurface supports gpu vsync callbacks.
    bool supports_gpu_vsync = false;
    // OutputSurface's orientation mode.
    OrientationMode orientation_mode = OrientationMode::kLogic;
    // Whether this OutputSurface supports direct composition layers.
    bool supports_dc_layers = false;
    // Set RGB10A2 overlay support flags true by force, which is used for
    // playing hdr video.
    // TODO(richard.li@intel.com): Remove this when Intel fixs its overlay caps.
    // checking bug in their driver.
    bool forces_rgb10a2_overlay_support_flags = false;
    // Whether this OutputSurface should skip DrawAndSwap(). This is true for
    // the unified display on Chrome OS. All drawing is handled by the physical
    // displays so the unified display should skip that work.
    bool skips_draw = false;
    // Indicates whether this surface will invalidate only the damage rect.
    // When this is false contents outside the damaged area might need to be
    // recomposited to the surface.
    bool only_invalidates_damage_rect = true;
    // Whether OutputSurface::GetTargetDamageBoundingRect is implemented and
    // will return a bounding rectangle of the target buffer invalidated area.
    bool supports_target_damage = false;
    // Whether the gpu supports surfaceless surface (equivalent of using buffer
    // queue).
    bool supports_surfaceless = false;
    // This is copied over from gpu feature info since there is no easy way to
    // share that out of skia output surface.
    bool android_surface_control_feature_enabled = false;
    // True if the buffer content will be preserved after presenting.
    bool preserve_buffer_content = false;
    // True if the SkiaOutputDevice will set
    // SwapBuffersCompleteParams::frame_buffer_damage_area for every
    // SwapBuffers complete callback.
    bool damage_area_from_skia_output_device = false;
    // This is the maximum size for RenderPass textures. No maximum size is
    // enforced if zero.
    int max_render_target_size = 0;

    // SkColorType for all supported buffer formats.
    SkColorType sk_color_types[static_cast<int>(gfx::BufferFormat::LAST) + 1] =
        {};
  };

  // Constructor for skia-based compositing.
  explicit OutputSurface(Type type);
  // Constructor for GL-based compositing.
  explicit OutputSurface(scoped_refptr<ContextProvider> context_provider);
  // Constructor for software compositing.
  explicit OutputSurface(std::unique_ptr<SoftwareOutputDevice> software_device);

  virtual ~OutputSurface();

  const Capabilities& capabilities() const { return capabilities_; }
  Type type() const { return type_; }

  // Obtain the 3d context or the software device associated with this output
  // surface. Either of these may return a null pointer, but not both.
  // In the event of a lost context, the entire output surface should be
  // recreated.
  ContextProvider* context_provider() const { return context_provider_.get(); }
  SoftwareOutputDevice* software_device() const {
    return software_device_.get();
  }

  // Downcasts to SkiaOutputSurface if it is one and returns nullptr otherwise.
  virtual SkiaOutputSurface* AsSkiaOutputSurface();

  void set_color_matrix(const SkMatrix44& color_matrix) {
    color_matrix_ = color_matrix;
  }
  const SkMatrix44& color_matrix() const { return color_matrix_; }

  // Only useful for GPU backend.
  virtual gpu::SurfaceHandle GetSurfaceHandle() const;

  virtual void BindToClient(OutputSurfaceClient* client) = 0;

  virtual void EnsureBackbuffer() = 0;
  virtual void DiscardBackbuffer() = 0;

  // Bind the default framebuffer for drawing to, only valid for GL backed
  // OutputSurfaces.
  virtual void BindFramebuffer() = 0;

  // Marks that the given rectangle will be drawn to on the default, bound
  // framebuffer. Only valid if |capabilities().supports_dc_layers| is true.
  virtual void SetDrawRectangle(const gfx::Rect& rect);

  // Enable or disable DC layers. Must be called before DC layers are scheduled.
  // Only valid if |capabilities().supports_dc_layers| is true.
  virtual void SetEnableDCLayers(bool enabled);

  // Returns true if a main image overlay plane should be scheduled.
  virtual bool IsDisplayedAsOverlayPlane() const = 0;

  // Get the texture for the main image's overlay.
  virtual unsigned GetOverlayTextureId() const = 0;

  // Returns the |mailbox| corresponding to the main image's overlay.
  virtual gpu::Mailbox GetOverlayMailbox() const;

  virtual void Reshape(const gfx::Size& size,
                       float device_scale_factor,
                       const gfx::ColorSpace& color_space,
                       gfx::BufferFormat format,
                       bool use_stencil) = 0;

  virtual bool HasExternalStencilTest() const = 0;
  virtual void ApplyExternalStencil() = 0;

  // Gives the GL internal format that should be used for calling CopyTexImage2D
  // when the framebuffer is bound via BindFramebuffer().
  virtual uint32_t GetFramebufferCopyTextureFormat() = 0;

  // Swaps the current backbuffer to the screen. For successful swaps, the
  // implementation must call OutputSurfaceClient::DidReceiveSwapBuffersAck()
  // after returning from this method in order to unblock the next frame.
  virtual void SwapBuffers(OutputSurfaceFrame frame) = 0;

  // Returns a rectangle whose contents may have changed since the current
  // buffer was last submitted and needs to be redrawn. For partial swap,
  // the contents outside this rectangle can be considered valid and do not need
  // to be redrawn.
  // In cases where partial swap is disabled, this method will still be called.
  // The direct renderer will union the returned rect with the rectangle of the
  // surface itself.
  // TODO(dcastagna): Consider making the following pure virtual.
  virtual gfx::Rect GetCurrentFramebufferDamage() const;

  // Updates the GpuFence associated with this surface. The id of a newly
  // created GpuFence is returned, or if an error occurs, or fences are not
  // supported, the special id of 0 (meaning "no fence") is returned.  In all
  // cases, any previously associated fence is destroyed. The returned fence id
  // corresponds to the GL id used by the CHROMIUM_gpu_fence GL extension and
  // can be passed directly to any related extension functions.
  virtual unsigned UpdateGpuFence() = 0;

  // Sets callback to receive updated vsync parameters after SwapBuffers() if
  // supported.
  virtual void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) = 0;

  // Set a callback for vsync signal from GPU service for begin frames.  The
  // callbacks must be received on the calling thread.
  virtual void SetGpuVSyncCallback(GpuVSyncCallback callback);

  // Enable or disable vsync callback based on whether begin frames are needed.
  virtual void SetGpuVSyncEnabled(bool enabled);

  // When the device is rotated, the scene prepared by the UI is in the logical
  // screen space as seen by the user. However, attempting to scanout a buffer
  // with its content in this logical space may be unsupported or inefficient
  // when rendered by the display hardware.
  //
  // In order to avoid this, this API provides the OutputSurface with the
  // transform/rotation that should be applied to the display compositor's
  // output. This is the same rotation as the physical rotation on the display.
  // In some cases, this is done natively by the graphics backend (
  // For instance, this is already done by GL drivers on Android. See
  // https://source.android.com/devices/graphics/implement#pre-rotation).
  //
  // If not supported natively, the OutputSurface should return the transform
  // needed in GetDisplayTransform for it to explicitly applied by the
  // compositor.
  virtual void SetDisplayTransformHint(gfx::OverlayTransform transform) = 0;
  virtual gfx::OverlayTransform GetDisplayTransform() = 0;

  virtual base::ScopedClosureRunner GetCacheBackBufferCb();

  // If set to true, the OutputSurface must deliver
  // OutputSurfaceclient::DidSwapWithSize notifications to its client.
  // OutputSurfaces which support delivering swap size notifications should
  // override this.
  virtual void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications);

  // Notifies surface that we want to measure viz-gpu latency for next draw.
  virtual void SetNeedsMeasureNextDrawLatency() {}

  // Updates timing info on the provided LatencyInfo when swap completes.
  static void UpdateLatencyInfoOnSwap(
      const gfx::SwapResponse& response,
      std::vector<ui::LatencyInfo>* latency_info);

  // This is used to share the same method to schedule task on the gpu thread
  // between the output surface and the overlay processor.
  // TODO(weiliangc): Consider making this outside of output surface and pass in
  // instead of passing it out here.
  virtual scoped_refptr<gpu::GpuTaskSchedulerHelper>
  GetGpuTaskSchedulerHelper() = 0;
  virtual gpu::MemoryTracker* GetMemoryTracker() = 0;

  // Notifies the OutputSurface of rate of content updates in frames per second.
  virtual void SetFrameRate(float frame_rate) {}

 protected:
  struct OutputSurface::Capabilities capabilities_;
  scoped_refptr<ContextProvider> context_provider_;
  std::unique_ptr<SoftwareOutputDevice> software_device_;

 private:
  const Type type_;
  SkMatrix44 color_matrix_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurface);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OUTPUT_SURFACE_H_
