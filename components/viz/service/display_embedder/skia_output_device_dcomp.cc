// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dcomp.h"

#include <tuple>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/cxx20_erase.h"
#include "base/debug/alias.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/dc_renderer_layer_params.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"

namespace viz {

namespace {
base::TimeTicks g_last_reshape_failure = base::TimeTicks();

NOINLINE void CheckForLoopFailures() {
  const auto threshold = base::Seconds(1);
  auto now = base::TimeTicks::Now();
  if (!g_last_reshape_failure.is_null() &&
      now - g_last_reshape_failure < threshold) {
    CHECK(false);
  }
  g_last_reshape_failure = now;
}

}  // namespace

// Holds reference needed to keep overlay textures alive.
// TODO(kylechar): We can probably merge OverlayData in with
// SkiaOutputSurfaceImplOnGpu overlay data.
class SkiaOutputDeviceDComp::OverlayData {
 public:
  explicit OverlayData(
      std::unique_ptr<gpu::OverlayImageRepresentation> representation)
      : representation_(std::move(representation)) {}

  ~OverlayData() = default;
  OverlayData(OverlayData&& other) = default;
  OverlayData& operator=(OverlayData&& other) {
    // `access_` must be overwritten before `representation_`.
    access_ = std::move(other.access_);
    representation_ = std::move(other.representation_);
    return *this;
  }

  gpu::OverlayImageRepresentation::ScopedReadAccess* BeginOverlayAccess() {
    DCHECK(representation_);
    access_ = representation_->BeginScopedReadAccess();
    DCHECK(access_);
    return access_.get();
  }

  void EndOverlayAccess() { access_.reset(); }

 private:
  std::unique_ptr<gpu::OverlayImageRepresentation> representation_;
  std::unique_ptr<gpu::OverlayImageRepresentation::ScopedReadAccess> access_;
};

SkiaOutputDeviceDComp::SkiaOutputDeviceDComp(
    gpu::MailboxManager* mailbox_manager,
    gpu::SharedImageRepresentationFactory* shared_image_representation_factory,
    gpu::SharedContextState* context_state,
    gl::GLSurface* gl_surface,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_state->gr_context(),
                       memory_tracker,
                       std::move(did_swap_buffer_complete_callback)),
      mailbox_manager_(mailbox_manager),
      shared_image_representation_factory_(shared_image_representation_factory),
      context_state_(context_state) {
  DCHECK(gl_surface->SupportsPostSubBuffer());
  DCHECK(!gl_surface->SupportsAsyncSwap());
  DCHECK(!feature_info->workarounds()
              .disable_post_sub_buffers_for_onscreen_surfaces);
  DCHECK(gl_surface->SupportsDCLayers());
  DCHECK_EQ(gl_surface->GetOrigin(), gfx::SurfaceOrigin::kTopLeft);
  DCHECK(gl_surface->SupportsGpuVSync());
  DCHECK(!gl_surface->SupportsCommitOverlayPlanes());

  capabilities_.uses_default_gl_framebuffer = true;
  capabilities_.output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities_.supports_post_sub_buffer = true;
  // DWM handles preserving the contents of the backbuffer in Present1, so we
  // don't need to have SkiaOutputSurface handle it.
  capabilities_.preserve_buffer_content = false;
  capabilities_.number_of_buffers =
      gl::DirectCompositionRootSurfaceBufferCount();
  capabilities_.supports_delegated_ink = gl_surface->SupportsDelegatedInk();
  if (feature_info->workarounds().supports_two_yuv_hardware_overlays) {
    capabilities_.supports_two_yuv_hardware_overlays = true;
  }
  capabilities_.pending_swap_params.max_pending_swaps =
      gl_surface->GetBufferCount() - 1;
  capabilities_.supports_commit_overlay_planes = false;
  capabilities_.supports_gpu_vsync = true;
  capabilities_.supports_dc_layers = true;

  DCHECK(context_state_);
  DCHECK(gl_surface);

  if (gl_surface->SupportsSwapTimestamps()) {
    gl_surface->SetEnableSwapTimestamps();

    // Changes to swap timestamp queries are only picked up when making current.
    context_state_->ReleaseCurrent(nullptr);
    context_state_->MakeCurrent(gl_surface);
  }

  DCHECK(context_state_->gr_context());
  DCHECK(context_state_->context());

  // SRGB
  constexpr SkColorType kSrgbColorType = kRGBA_8888_SkColorType;
  // TODO(tangm): switch to kRGB_888x_SkColorType
  constexpr SkColorType kSrgbColorTypeOpaque = kRGBA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      kSrgbColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBX_8888)] =
      kSrgbColorTypeOpaque;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      kSrgbColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRX_8888)] =
      kSrgbColorTypeOpaque;
  // HDR10
  capabilities_
      .sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_1010102)] =
      kRGBA_1010102_SkColorType;
  // scRGB linear
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_F16)] =
      kRGBA_F16_SkColorType;
}

SkiaOutputDeviceDComp::~SkiaOutputDeviceDComp() = default;

void SkiaOutputDeviceDComp::SwapBuffers(BufferPresentedCallback feedback,
                                        OutputSurfaceFrame frame) {
  PostSubBuffer(gfx::Rect(GetRootSurfaceSize()), std::move(feedback),
                std::move(frame));
}

void SkiaOutputDeviceDComp::PostSubBuffer(const gfx::Rect& rect,
                                          BufferPresentedCallback feedback,
                                          OutputSurfaceFrame frame) {
  StartSwapBuffers({});

  gfx::SwapResult result =
      DoPostSubBuffer(rect, std::move(feedback), frame.data);

  // Remove entries from |overlays_| for textures that weren't scheduled as an
  // overlay this frame.
  if (!overlays_.empty()) {
    base::EraseIf(overlays_, [this](auto& entry) {
      const gpu::Mailbox& mailbox = entry.first;
      return !scheduled_overlay_mailboxes_.contains(mailbox);
    });
    scheduled_overlay_mailboxes_.clear();
    // End access for the remaining overlays that were scheduled this frame.
    for (auto& kv : overlays_)
      kv.second.EndOverlayAccess();
  }

  FinishSwapBuffers(gfx::SwapCompletionResult(result), GetRootSurfaceSize(),
                    std::move(frame));
}

void SkiaOutputDeviceDComp::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  for (auto& dc_layer : overlays) {
    auto params = std::make_unique<ui::DCRendererLayerParams>();
    // Get GLImages for DC layer textures.
    bool success = true;
    for (size_t i = 0; i < DCLayerOverlay::kNumResources; ++i) {
      const gpu::Mailbox& mailbox = dc_layer.mailbox[i];
      if (i > 0 && mailbox.IsZero())
        break;

      auto* read_access = BeginOverlayAccess(mailbox);
      if (!read_access) {
        success = false;
        break;
      }

      if (auto dcomp_surface_proxy = read_access->GetDCOMPSurfaceProxy()) {
        params->dcomp_surface_proxy = std::move(dcomp_surface_proxy);
      } else if (auto* image = read_access->gl_image()) {
        image->SetColorSpace(dc_layer.color_space);
        params->images[i] = std::move(image);
      } else {
        success = false;
        break;
      }

      scheduled_overlay_mailboxes_.insert(mailbox);
    }

    if (!success) {
      DLOG(ERROR) << "Failed to get GLImage for DC layer.";
      continue;
    }

    params->z_order = dc_layer.z_order;
    params->content_rect = dc_layer.content_rect;
    params->quad_rect = dc_layer.quad_rect;
    DCHECK(dc_layer.transform.IsFlat());
    params->transform = dc_layer.transform;
    params->clip_rect = dc_layer.clip_rect;
    params->protected_video_type = dc_layer.protected_video_type;
    params->hdr_metadata = dc_layer.hdr_metadata;
    params->is_video_fullscreen_letterboxing =
        dc_layer.is_video_fullscreen_letterboxing;

    // Schedule DC layer overlay to be presented at next SwapBuffers().
    if (!ScheduleDCLayer(std::move(params)))
      DLOG(ERROR) << "ScheduleDCLayer failed";
  }
}

gpu::OverlayImageRepresentation::ScopedReadAccess*
SkiaOutputDeviceDComp::BeginOverlayAccess(const gpu::Mailbox& mailbox) {
  auto it = overlays_.find(mailbox);
  if (it != overlays_.end())
    return it->second.BeginOverlayAccess();

  auto overlay = shared_image_representation_factory_->ProduceOverlay(mailbox);
  if (!overlay)
    return nullptr;

  std::tie(it, std::ignore) = overlays_.emplace(mailbox, std::move(overlay));
  return it->second.BeginOverlayAccess();
}

SkiaOutputDeviceDCompGLSurface::SkiaOutputDeviceDCompGLSurface(
    gpu::MailboxManager* mailbox_manager,
    gpu::SharedImageRepresentationFactory* shared_image_representation_factory,
    gpu::SharedContextState* context_state,
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDeviceDComp(mailbox_manager,
                            shared_image_representation_factory,
                            context_state,
                            gl_surface.get(),
                            std::move(feature_info),
                            memory_tracker,
                            std::move(did_swap_buffer_complete_callback)),
      gl_surface_(std::move(gl_surface)) {}

SkiaOutputDeviceDCompGLSurface::~SkiaOutputDeviceDCompGLSurface() {
  // gl_surface_ will be destructed soon.
  memory_type_tracker_->TrackMemFree(backbuffer_estimated_size_);
}

bool SkiaOutputDeviceDCompGLSurface::Reshape(
    const SkSurfaceCharacterization& characterization,
    const gfx::ColorSpace& color_space,
    float device_scale_factor,
    gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  const gfx::Size size = gfx::SkISizeToSize(characterization.dimensions());
  const SkColorType color_type = characterization.colorType();
  const bool has_alpha =
      !SkAlphaTypeIsOpaque(characterization.imageInfo().alphaType());

  if (!gl_surface_->Resize(size, device_scale_factor, color_space, has_alpha)) {
    CheckForLoopFailures();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
    return false;
  }
  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};

  GrGLFramebufferInfo framebuffer_info = {0};
  DCHECK_EQ(gl_surface_->GetBackingFramebufferObject(), 0u);

  switch (color_type) {
    case kRGBA_8888_SkColorType:
      framebuffer_info.fFormat = GL_RGBA8;
      break;
    case kRGB_888x_SkColorType:
      framebuffer_info.fFormat = GL_RGB8;
      break;
    case kRGB_565_SkColorType:
      framebuffer_info.fFormat = GL_RGB565;
      break;
    case kRGBA_1010102_SkColorType:
      framebuffer_info.fFormat = GL_RGB10_A2_EXT;
      break;
    case kRGBA_F16_SkColorType:
      framebuffer_info.fFormat = GL_RGBA16F;
      break;
    default:
      NOTREACHED() << "color_type: " << color_type;
  }

  GrBackendRenderTarget render_target(size.width(), size.height(),
                                      characterization.sampleCount(),
                                      /*stencilBits=*/0, framebuffer_info);
  auto origin = (gl_surface_->GetOrigin() == gfx::SurfaceOrigin::kTopLeft)
                    ? kTopLeft_GrSurfaceOrigin
                    : kBottomLeft_GrSurfaceOrigin;
  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      context_state_->gr_context(), render_target, origin, color_type,
      characterization.refColorSpace(), &surface_props);
  if (!sk_surface_) {
    LOG(ERROR) << "Couldn't create surface:"
               << "\n  abandoned()="
               << context_state_->gr_context()->abandoned()
               << "\n  color_type=" << color_type
               << "\n  framebuffer_info.fFBOID=" << framebuffer_info.fFBOID
               << "\n  framebuffer_info.fFormat=" << framebuffer_info.fFormat
               << "\n  color_space=" << color_space.ToString()
               << "\n  size=" << size.ToString();
    CheckForLoopFailures();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
  }

  memory_type_tracker_->TrackMemFree(backbuffer_estimated_size_);
  GLenum format = gpu::gles2::TextureManager::ExtractFormatFromStorageFormat(
      framebuffer_info.fFormat);
  GLenum type = gpu::gles2::TextureManager::ExtractTypeFromStorageFormat(
      framebuffer_info.fFormat);
  uint32_t estimated_size;
  gpu::gles2::GLES2Util::ComputeImageDataSizes(
      size.width(), size.height(), 1 /* depth */, format, type,
      4 /* alignment */, &estimated_size, nullptr, nullptr);
  backbuffer_estimated_size_ = estimated_size * gl_surface_->GetBufferCount();
  memory_type_tracker_->TrackMemAlloc(backbuffer_estimated_size_);

  return !!sk_surface_;
}

bool SkiaOutputDeviceDCompGLSurface::SetDrawRectangle(
    const gfx::Rect& draw_rectangle) {
  return gl_surface_->SetDrawRectangle(draw_rectangle);
}

void SkiaOutputDeviceDCompGLSurface::SetEnableDCLayers(bool enable) {
  gl_surface_->SetEnableDCLayers(enable);
}

void SkiaOutputDeviceDCompGLSurface::SetGpuVSyncEnabled(bool enabled) {
  gl_surface_->SetGpuVSyncEnabled(enabled);
}

SkSurface* SkiaOutputDeviceDCompGLSurface::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(sk_surface_);
  return sk_surface_.get();
}

void SkiaOutputDeviceDCompGLSurface::EndPaint() {}

bool SkiaOutputDeviceDCompGLSurface::ScheduleDCLayer(
    std::unique_ptr<ui::DCRendererLayerParams> params) {
  return gl_surface_->ScheduleDCLayer(std::move(params));
}

gfx::Size SkiaOutputDeviceDCompGLSurface::GetRootSurfaceSize() const {
  return gl_surface_->GetSize();
}

gfx::SwapResult SkiaOutputDeviceDCompGLSurface::DoPostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    gl::FrameData data) {
  return gl_surface_->PostSubBuffer(rect.x(), rect.y(), rect.width(),
                                    rect.height(), std::move(feedback), data);
}

}  // namespace viz
