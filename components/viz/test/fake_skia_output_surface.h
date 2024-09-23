// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_SKIA_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_TEST_FAKE_SKIA_OUTPUT_SURFACE_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/gpu/buildflags.h"

namespace viz {

class FakeSkiaOutputSurface : public SkiaOutputSurface {
  using SharedImagePurgeableCallback =
      base::RepeatingCallback<void(const gpu::Mailbox&, bool)>;

 public:
  static std::unique_ptr<FakeSkiaOutputSurface> Create3d() {
    auto provider = TestContextProvider::Create();
    provider->BindToCurrentSequence();
    return base::WrapUnique(new FakeSkiaOutputSurface(std::move(provider)));
  }

  static std::unique_ptr<FakeSkiaOutputSurface> Create3d(
      scoped_refptr<ContextProvider> provider) {
    return base::WrapUnique(new FakeSkiaOutputSurface(std::move(provider)));
  }

  FakeSkiaOutputSurface(const FakeSkiaOutputSurface&) = delete;
  FakeSkiaOutputSurface& operator=(const FakeSkiaOutputSurface&) = delete;

  ~FakeSkiaOutputSurface() override;

  // OutputSurface implementation:
  void BindToClient(OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const ReshapeParams& params) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;

  // SkiaOutputSurface implementation:
  SkCanvas* BeginPaintCurrentFrame() override;
  void SwapBuffersSkipped(const gfx::Rect root_pass_damage_rect) override {}
  SkCanvas* BeginPaintRenderPass(const AggregatedRenderPassId& id,
                                 const gfx::Size& surface_size,
                                 SharedImageFormat format,
                                 RenderPassAlphaType alpha_type,
                                 skgpu::Mipmapped mipmap,
                                 bool scanout_dcomp_surface,
                                 sk_sp<SkColorSpace> color_space,
                                 bool is_overlay,
                                 const gpu::Mailbox& mailbox) override;
  SkCanvas* RecordOverdrawForCurrentPaint() override;
  void EndPaint(
      base::OnceClosure on_finished,
      base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb,
      const gfx::Rect& update_rect,
      bool is_overlay) override;
  void MakePromiseSkImage(ImageContext* image_context,
                          const gfx::ColorSpace& yuv_color_space,
                          bool force_rgbx) override;
  sk_sp<SkImage> MakePromiseSkImageFromRenderPass(
      const AggregatedRenderPassId& id,
      const gfx::Size& size,
      SharedImageFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space,
      const gpu::Mailbox& mailbox) override;
  void RemoveRenderPassResource(
      std::vector<AggregatedRenderPassId> ids) override;
  void ScheduleOverlays(OverlayList overlays,
                        std::vector<gpu::SyncToken> sync_tokens) override {}
  void CopyOutput(const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request,
                  const gpu::Mailbox& mailbox) override;
  void AddContextLostObserver(ContextLostObserver* observer) override;
  void RemoveContextLostObserver(ContextLostObserver* observer) override;
  gpu::SyncToken Flush() override;
  void PreserveChildSurfaceControls() override {}
  gpu::Mailbox CreateSharedImage(SharedImageFormat format,
                                 const gfx::Size& size,
                                 const gfx::ColorSpace& color_space,
                                 RenderPassAlphaType alpha_type,
                                 gpu::SharedImageUsageSet usage,
                                 std::string_view debug_label,
                                 gpu::SurfaceHandle surface_handle) override;
  gpu::Mailbox CreateSolidColorSharedImage(
      const SkColor4f& color,
      const gfx::ColorSpace& color_space) override;
  void DestroySharedImage(const gpu::Mailbox& mailbox) override {}
  void SetSharedImagePurgeable(const gpu::Mailbox& mailbox,
                               bool purgeable) override;
  bool SupportsBGRA() const override;

  // ExternalUseClient implementation:
  gpu::SyncToken ReleaseImageContexts(
      const std::vector<std::unique_ptr<ImageContext>> image_contexts) override;
  std::unique_ptr<ImageContext> CreateImageContext(
      const gpu::MailboxHolder& holder,
      const gfx::Size& size,
      SharedImageFormat format,
      bool concurrent_reads,
      const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
      sk_sp<SkColorSpace> color_space,
      bool raw_draw_if_possible) override;

  gpu::SharedImageInterface* GetSharedImageInterface();

  void SetSharedImagePurgeableCallback(SharedImagePurgeableCallback callback) {
    set_purgeable_callback_ = std::move(callback);
  }

  // If set true, callbacks triggering will be in a reverse order as SignalQuery
  // calls.
  void SetOutOfOrderCallbacks(bool out_of_order_callbacks);

  void ScheduleGpuTaskForTesting(
      base::OnceClosure callback,
      std::vector<gpu::SyncToken> sync_tokens) override;
  void CheckAsyncWorkCompletionForTesting() override;

  void UsePlatformDelegatedInkForTesting() {
    capabilities_.supports_delegated_ink = true;
  }

  gfx::DelegatedInkMetadata* last_delegated_ink_metadata() const {
    return last_delegated_ink_metadata_.get();
  }

  void InitDelegatedInkPointRendererReceiver(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver) override;

  bool ContainsDelegatedInkPointRendererReceiverForTesting() const {
    return delegated_ink_renderer_receiver_arrived_;
  }

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
  void DetileOverlay(gpu::Mailbox input,
                     const gfx::Size& input_visible_size,
                     gpu::SyncToken input_sync_token,
                     gpu::Mailbox output,
                     const gfx::RectF& display_rect,
                     const gfx::RectF& crop_rect,
                     gfx::OverlayTransform transform,
                     bool is_10bit) override {}

  void CleanupImageProcessor() override {}
#endif

 protected:
  explicit FakeSkiaOutputSurface(
      scoped_refptr<ContextProvider> context_provider);

 private:
  ContextProvider* context_provider() { return context_provider_.get(); }
  GrDirectContext* gr_context() { return context_provider()->GrContext(); }

  gpu::SyncToken GenerateSyncToken();

  bool GetGrBackendTexture(const ImageContext& image_context,
                           GrBackendTexture* backend_texture);
  void SwapBuffersAck();

  // Provided as a release callback for CopyOutputRequest.
  void DestroyCopyOutputTexture(
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      const gpu::SyncToken& sync_token,
      bool is_lost);

  scoped_refptr<ContextProvider> context_provider_;
  raw_ptr<OutputSurfaceClient> client_ = nullptr;

  uint64_t next_sync_fence_release_ = 1;

  // The current render pass id set by BeginPaintRenderPass.
  AggregatedRenderPassId current_render_pass_id_;

  // SkSurfaces for render passes, sk_surfaces_[0] is the root surface.
  base::flat_map<AggregatedRenderPassId, sk_sp<SkSurface>> sk_surfaces_;

  // Map from mailboxes to render pass ids.
  base::flat_map<gpu::Mailbox, AggregatedRenderPassId> mailbox_pass_ids_;

  // Most recent delegated ink metadata to have arrived via a SwapBuffers call.
  std::unique_ptr<gfx::DelegatedInkMetadata> last_delegated_ink_metadata_;

  // Flag to mark if a pending delegated ink renderer mojo receiver has arrived
  // here or not. Used in testing to confirm that the pending receiver is
  // correctly routed towards gpu main when the platform supports delegated ink.
  bool delegated_ink_renderer_receiver_arrived_ = false;

  SharedImagePurgeableCallback set_purgeable_callback_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<FakeSkiaOutputSurface> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_SKIA_OUTPUT_SURFACE_H_
