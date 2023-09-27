// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_SKIA_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_TEST_FAKE_SKIA_OUTPUT_SURFACE_H_

#include <memory>
#include <string>
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
#include "gpu/command_buffer/common/sync_token.h"

namespace viz {

class FakeSkiaOutputSurface : public SkiaOutputSurface {
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
  void ScheduleOutputSurfaceAsOverlay(
      OverlayProcessorInterface::OutputSurfaceOverlayPlane output_surface_plane)
      override;
  bool IsDisplayedAsOverlayPlane() const override;
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;

  // SkiaOutputSurface implementation:
  SkCanvas* BeginPaintCurrentFrame() override;
  sk_sp<SkImage> MakePromiseSkImageFromYUV(
      const std::vector<ImageContext*>& contexts,
      sk_sp<SkColorSpace> image_color_space,
      SkYUVAInfo::PlaneConfig plane_config,
      SkYUVAInfo::Subsampling subsampling) override;
  void SwapBuffersSkipped(const gfx::Rect root_pass_damage_rect) override {}
  SkCanvas* BeginPaintRenderPass(const AggregatedRenderPassId& id,
                                 const gfx::Size& surface_size,
                                 SharedImageFormat format,
                                 RenderPassAlphaType alpha_type,
                                 bool mipmap,
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
                          const gfx::ColorSpace& yuv_color_space) override;
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
#if BUILDFLAG(IS_WIN)
  void SetEnableDCLayers(bool enable) override {}
#endif
  void CopyOutput(const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request,
                  const gpu::Mailbox& mailbox) override;
  void AddContextLostObserver(ContextLostObserver* observer) override;
  void RemoveContextLostObserver(ContextLostObserver* observer) override;
  gpu::SyncToken Flush() override;
  bool EnsureMinNumberOfBuffers(int n) override;
  void PreserveChildSurfaceControls() override {}
  gpu::Mailbox CreateSharedImage(SharedImageFormat format,
                                 const gfx::Size& size,
                                 const gfx::ColorSpace& color_space,
                                 RenderPassAlphaType alpha_type,
                                 uint32_t usage,
                                 base::StringPiece debug_label,
                                 gpu::SurfaceHandle surface_handle) override;
  gpu::Mailbox CreateSolidColorSharedImage(
      const SkColor4f& color,
      const gfx::ColorSpace& color_space) override;
  void DestroySharedImage(const gpu::Mailbox& mailbox) override {}
  bool SupportsBGRA() const override;

  // ExternalUseClient implementation:
  gpu::SyncToken ReleaseImageContexts(
      const std::vector<std::unique_ptr<ImageContext>> image_contexts) override;
  std::unique_ptr<ImageContext> CreateImageContext(
      const gpu::MailboxHolder& holder,
      const gfx::Size& size,
      SharedImageFormat format,
      bool concurrent_reads,
      const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
      sk_sp<SkColorSpace> color_space,
      bool raw_draw_if_possible) override;

  gpu::SharedImageInterface* GetSharedImageInterface();

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
  void DestroyCopyOutputTexture(const gpu::Mailbox& mailbox,
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

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<FakeSkiaOutputSurface> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_SKIA_OUTPUT_SURFACE_H_
