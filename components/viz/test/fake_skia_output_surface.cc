// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_skia_output_surface.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_utils.h"

namespace viz {

FakeSkiaOutputSurface::FakeSkiaOutputSurface(
    scoped_refptr<ContextProvider> context_provider)
    : context_provider_(std::move(context_provider)) {}

FakeSkiaOutputSurface::~FakeSkiaOutputSurface() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FakeSkiaOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void FakeSkiaOutputSurface::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FakeSkiaOutputSurface::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FakeSkiaOutputSurface::Reshape(const ReshapeParams& params) {
  auto& sk_surface = sk_surfaces_[AggregatedRenderPassId{0}];
  SkColorType color_type = kRGBA_8888_SkColorType;
  SkImageInfo image_info = SkImageInfo::Make(
      params.size.width(), params.size.height(), color_type,
      kPremul_SkAlphaType, params.color_space.ToSkColorSpace());
  sk_surface =
      SkSurfaces::RenderTarget(gr_context(), skgpu::Budgeted::kNo, image_info);

  DCHECK(sk_surface);
}

void FakeSkiaOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (frame.delegated_ink_metadata)
    last_delegated_ink_metadata_ = std::move(frame.delegated_ink_metadata);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeSkiaOutputSurface::SwapBuffersAck,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeSkiaOutputSurface::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  NOTIMPLEMENTED();
}

void FakeSkiaOutputSurface::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  NOTIMPLEMENTED();
}

gfx::OverlayTransform FakeSkiaOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

SkCanvas* FakeSkiaOutputSurface::BeginPaintCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto& sk_surface = sk_surfaces_[AggregatedRenderPassId{0}];
  DCHECK(sk_surface);
  DCHECK_EQ(current_render_pass_id_, AggregatedRenderPassId{0u});
  return sk_surface->getCanvas();
}

void FakeSkiaOutputSurface::MakePromiseSkImage(
    ImageContext* image_context,
    const gfx::ColorSpace& yuv_color_space,
    bool force_rgbx) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (image_context->has_image()) {
    return;
  }

  if (image_context->format().is_multi_plane()) {
    NOTIMPLEMENTED();
    return;
  }

  GrBackendTexture backend_texture;
  if (!GetGrBackendTexture(*image_context, &backend_texture)) {
    DLOG(ERROR) << "Failed to GetGrBackendTexture from mailbox.";
    return;
  }

  auto sk_color_type =
      ToClosestSkColorType(true /* gpu_compositing */, image_context->format());
  image_context->SetImage(
      SkImages::BorrowTextureFrom(gr_context(), backend_texture,
                                  kTopLeft_GrSurfaceOrigin, sk_color_type,
                                  image_context->alpha_type(),
                                  image_context->color_space()),
      {backend_texture.getBackendFormat()});
}

gpu::SyncToken FakeSkiaOutputSurface::ReleaseImageContexts(
    std::vector<std::unique_ptr<ImageContext>> image_contexts) {
  return gpu::SyncToken();
}

std::unique_ptr<ExternalUseClient::ImageContext>
FakeSkiaOutputSurface::CreateImageContext(
    const gpu::MailboxHolder& holder,
    const gfx::Size& size,
    SharedImageFormat format,
    bool concurrent_reads,
    const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space,
    bool raw_draw_if_possible) {
  return std::make_unique<ExternalUseClient::ImageContext>(
      holder, size, format, ycbcr_info, std::move(color_space));
}

SkCanvas* FakeSkiaOutputSurface::BeginPaintRenderPass(
    const AggregatedRenderPassId& id,
    const gfx::Size& surface_size,
    SharedImageFormat format,
    RenderPassAlphaType alpha_type,
    skgpu::Mipmapped,
    bool scanout_dcomp_surface,
    sk_sp<SkColorSpace> color_space,
    bool is_overlay,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK_EQ(current_render_pass_id_, AggregatedRenderPassId{0u});

  mailbox_pass_ids_.insert_or_assign(mailbox, id);
  auto& sk_surface = sk_surfaces_[id];

  if (!sk_surface) {
    SkColorType color_type =
        ToClosestSkColorType(true /* gpu_compositing */, format);
    SkImageInfo image_info = SkImageInfo::Make(
        surface_size.width(), surface_size.height(), color_type,
        kPremul_SkAlphaType, std::move(color_space));
    sk_surface = SkSurfaces::RenderTarget(gr_context(), skgpu::Budgeted::kNo,
                                          image_info);
  }
  return sk_surface->getCanvas();
}

SkCanvas* FakeSkiaOutputSurface::RecordOverdrawForCurrentPaint() {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeSkiaOutputSurface::EndPaint(
    base::OnceClosure on_finished,
    base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb,
    const gfx::Rect& update_rect,
    bool is_overlay) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  skgpu::ganesh::FlushAndSubmit(sk_surfaces_[current_render_pass_id_]);
  current_render_pass_id_ = AggregatedRenderPassId{0};

  if (on_finished)
    std::move(on_finished).Run();
  if (return_release_fence_cb)
    std::move(return_release_fence_cb).Run(gfx::GpuFenceHandle());
}

sk_sp<SkImage> FakeSkiaOutputSurface::MakePromiseSkImageFromRenderPass(
    const AggregatedRenderPassId& id,
    const gfx::Size& size,
    SharedImageFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = sk_surfaces_.find(id);
  CHECK(it != sk_surfaces_.end(), base::NotFatalUntil::M130);
  return it->second->makeImageSnapshot();
}

void FakeSkiaOutputSurface::RemoveRenderPassResource(
    std::vector<AggregatedRenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());

  for (const auto& id : ids) {
    auto it = sk_surfaces_.find(id);
    CHECK(it != sk_surfaces_.end(), base::NotFatalUntil::M130);
    sk_surfaces_.erase(it);
  }

  // Erase mailbox mappings that exist for these ids.
  base::EraseIf(mailbox_pass_ids_, [&ids](auto& entry) {
    for (auto& id : ids) {
      if (id == entry.second) {
        return true;
      }
    }
    return false;
  });
}

void FakeSkiaOutputSurface::CopyOutput(
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = mailbox_pass_ids_.find(mailbox);
  DCHECK(mailbox.IsZero() || it != mailbox_pass_ids_.end());
  AggregatedRenderPassId id =
      mailbox.IsZero() ? AggregatedRenderPassId(0) : it->second;

  DCHECK(sk_surfaces_.find(id) != sk_surfaces_.end());
  auto* surface = sk_surfaces_[id].get();
  if (request->result_format() != CopyOutputResult::Format::RGBA ||
      request->is_scaled() ||
      geometry.result_bounds != geometry.result_selection) {
    // TODO(crbug.com/40483986): Complete the implementation for all request
    // types, scaling, etc.
    NOTIMPLEMENTED();
    return;
  }

  if (request->result_destination() ==
      CopyOutputResult::Destination::kNativeTextures) {
    // NOTE: This implementation is incomplete and doesn't copy anything into
    // the mailbox, but currently the only tests that use this don't actually
    // check the returned texture data. A corollary to this fact is that the
    // usage flags passed in are currently arbitrary, since callers don't
    // actually do anything meaningful with the SharedImage.
    auto* sii = GetSharedImageInterface();
    auto client_shared_image =
        sii->CreateSharedImage({SinglePlaneFormat::kRGBA_8888,
                                geometry.result_selection.size(), color_space,
                                gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                                    gpu::SHARED_IMAGE_USAGE_GLES2_WRITE,
                                "CopyOutput"},
                               gpu::kNullSurfaceHandle);
    CHECK(client_shared_image);
    gpu::Mailbox local_mailbox = client_shared_image->mailbox();

    CopyOutputResult::ReleaseCallbacks release_callbacks;
    release_callbacks.push_back(
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &FakeSkiaOutputSurface::DestroyCopyOutputTexture,
            weak_ptr_factory_.GetWeakPtr(), std::move(client_shared_image))));

    request->SendResult(std::make_unique<CopyOutputTextureResult>(
        CopyOutputResult::Format::RGBA, geometry.result_bounds,
        CopyOutputResult::TextureResult(local_mailbox, color_space),
        std::move(release_callbacks)));
    return;
  }

  GrDirectContext* direct = GrAsDirectContext(gr_context());
  auto copy_image = surface->makeImageSnapshot()->makeSubset(
      direct, RectToSkIRect(geometry.sampling_bounds));
  // Send copy request by copying into a bitmap.
  SkBitmap bitmap;
  copy_image->asLegacyBitmap(&bitmap);
  // TODO(crbug.com/40554816): Plumb color space throughout SkiaRenderer up to
  // the SkSurface/SkImage here. Until then, play "musical chairs" with the
  // SkPixelRef to hack-in the RenderPass's |color_space|.
  sk_sp<SkPixelRef> pixels(SkSafeRef(bitmap.pixelRef()));
  SkIPoint origin = bitmap.pixelRefOrigin();
  bitmap.setInfo(bitmap.info().makeColorSpace(color_space.ToSkColorSpace()),
                 bitmap.rowBytes());
  bitmap.setPixelRef(std::move(pixels), origin.x(), origin.y());
  request->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
      geometry.result_bounds, std::move(bitmap)));
}

gpu::SharedImageInterface* FakeSkiaOutputSurface::GetSharedImageInterface() {
  return context_provider_->SharedImageInterface();
}

void FakeSkiaOutputSurface::AddContextLostObserver(
    ContextLostObserver* observer) {}

void FakeSkiaOutputSurface::RemoveContextLostObserver(
    ContextLostObserver* observer) {}

gpu::SyncToken FakeSkiaOutputSurface::Flush() {
  return GenerateSyncToken();
}

void FakeSkiaOutputSurface::SetOutOfOrderCallbacks(
    bool out_of_order_callbacks) {
  TestContextSupport* support =
      static_cast<TestContextSupport*>(context_provider()->ContextSupport());
  support->set_out_of_order_callbacks(out_of_order_callbacks);
}

gpu::SyncToken FakeSkiaOutputSurface::GenerateSyncToken() {
  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
      gpu::CommandBufferId(), ++next_sync_fence_release_);
  sync_token.SetVerifyFlush();
  return sync_token;
}

bool FakeSkiaOutputSurface::GetGrBackendTexture(
    const ImageContext& image_context,
    GrBackendTexture* backend_texture) {
  DCHECK(!image_context.mailbox_holder().mailbox.IsZero());

  auto* gl = context_provider()->ContextGL();
  gl->WaitSyncTokenCHROMIUM(
      image_context.mailbox_holder().sync_token.GetConstData());
  auto texture_id = gl->CreateAndTexStorage2DSharedImageCHROMIUM(
      image_context.mailbox_holder().mailbox.name);
  auto gl_format_desc = gpu::GLFormatCaps().ToGLFormatDesc(
      image_context.format(), /*plane_index=*/0);
  GrGLTextureInfo gl_texture_info = {
      image_context.mailbox_holder().texture_target, texture_id,
      gl_format_desc.storage_internal_format};
  *backend_texture = GrBackendTextures::MakeGL(
      image_context.size().width(), image_context.size().height(),
      skgpu::Mipmapped::kNo, gl_texture_info);
  return true;
}

void FakeSkiaOutputSurface::SwapBuffersAck() {
  base::TimeTicks now = base::TimeTicks::Now();
  gpu::SwapBuffersCompleteParams params;
  params.swap_response.timings = {now, now};
  params.swap_response.result = gfx::SwapResult::SWAP_ACK;
  client_->DidReceiveSwapBuffersAck(params,
                                    /*release_fence=*/gfx::GpuFenceHandle());
  client_->DidReceivePresentationFeedback({now, base::TimeDelta(), 0});
}

void FakeSkiaOutputSurface::DestroyCopyOutputTexture(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  GetSharedImageInterface()->DestroySharedImage(sync_token,
                                                std::move(shared_image));
}

void FakeSkiaOutputSurface::ScheduleGpuTaskForTesting(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> sync_tokens) {
  NOTIMPLEMENTED();
}

void FakeSkiaOutputSurface::CheckAsyncWorkCompletionForTesting() {
  NOTIMPLEMENTED();
}

void FakeSkiaOutputSurface::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  delegated_ink_renderer_receiver_arrived_ = true;
}

gpu::Mailbox FakeSkiaOutputSurface::CreateSharedImage(
    SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    RenderPassAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string_view debug_label,
    gpu::SurfaceHandle surface_handle) {
  return gpu::Mailbox::Generate();
}

gpu::Mailbox FakeSkiaOutputSurface::CreateSolidColorSharedImage(
    const SkColor4f& color,
    const gfx::ColorSpace& color_space) {
  return gpu::Mailbox::Generate();
}

void FakeSkiaOutputSurface::SetSharedImagePurgeable(const gpu::Mailbox& mailbox,
                                                    bool purgeable) {
  if (set_purgeable_callback_) {
    set_purgeable_callback_.Run(mailbox, purgeable);
  }
}

bool FakeSkiaOutputSurface::SupportsBGRA() const {
  return true;
}

}  // namespace viz
