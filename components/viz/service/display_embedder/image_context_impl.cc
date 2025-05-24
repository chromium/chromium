// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/image_context_impl.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/graphite_shared_context.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "skia/buildflags.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrContextThreadSafeProxy.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnTypes.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CreateFallbackImageResult {
  kSuccess = 0,
  kFailedPrefersExternalSampler = 1,
  kFailedYcbcrMismatch = 2,
  kFailedExternalTexture = 3,
  kFailedInvalidTextureInfo = 4,
  kFailedCreateTexture = 5,
  kMaxValue = kFailedCreateTexture
};

const char* CreateFallbackImageResultToString(
    CreateFallbackImageResult result) {
  switch (result) {
    case CreateFallbackImageResult::kSuccess:
      return "Success";
    case CreateFallbackImageResult::kFailedPrefersExternalSampler:
      return "FailedPrefersExternalSampler";
    case CreateFallbackImageResult::kFailedYcbcrMismatch:
      return "FailedYcbcrMismatch";
    case CreateFallbackImageResult::kFailedExternalTexture:
      return "FailedExternalTexture";
    case CreateFallbackImageResult::kFailedInvalidTextureInfo:
      return "FailedInvalidTextureInfo";
    case CreateFallbackImageResult::kFailedCreateTexture:
      return "FailedCreateTexture";
  }
}

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(SKIA_USE_DAWN)
bool DawnYCbCrVkDescriptorsAreCompatible(const wgpu::YCbCrVkDescriptor& left,
                                         const wgpu::YCbCrVkDescriptor& right) {
  // NOTE: We deliberately do not compare the swizzle components as those
  // components are not plumbed through the Chrome-level information and thus
  // could cause spurious equality failures. By the Vulkan spec, those
  // components should not be set for external formats, but some drivers do not
  // adhere to the spec here.
  // Mismatch of model, range and chroma fields happens often enough to be
  // problematic if we skip drawing the video for those frames. While the video
  // may not draw 100% correctly it will still be better than not drawing it at
  // all.
  if (left.vkFormat != right.vkFormat) {
    return false;
  }
  if (left.forceExplicitReconstruction != right.forceExplicitReconstruction) {
    return false;
  }
  if (left.externalFormat != right.externalFormat) {
    return false;
  }
  return true;
}
#endif

SkColor4f GetFallbackColorForPlane(viz::SharedImageFormat format,
                                   int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  // For DCHECKed builds return: 1) kRed for single planar formats; 2) kWhite
  // for all multiplanar format planes that translates to Purple/Magenta color
  // in RGB space. For non-DCHECKed builds return: 1) kWhite for single planar
  // formats; 2) kWhite for Y, A planes and kGray for U, V, UV planes that
  // translates to White color in RGB colorspace.
#if DCHECK_IS_ON()
  return format.is_single_plane() ? SkColors::kRed : SkColors::kWhite;
#else
  if (format.is_single_plane())
    return SkColors::kWhite;
  switch (format.plane_config()) {
    case viz::SharedImageFormat::PlaneConfig::kY_U_V:
    case viz::SharedImageFormat::PlaneConfig::kY_V_U:
      return plane_index == 0 ? SkColors::kWhite : SkColors::kGray;
    case viz::SharedImageFormat::PlaneConfig::kY_U_V_A:
      return (plane_index == 0 || plane_index == 3) ? SkColors::kWhite
                                                    : SkColors::kGray;
    case viz::SharedImageFormat::PlaneConfig::kY_UV:
      return plane_index == 0 ? SkColors::kWhite : SkColors::kGray;
    case viz::SharedImageFormat::PlaneConfig::kY_UV_A:
      return plane_index == 1 ? SkColors::kGray : SkColors::kWhite;
  }
#endif
}

}  // namespace

namespace viz {

ImageContextImpl::ImageContextImpl(const TransferableResource& resource,
                                   bool maybe_concurrent_reads,
                                   bool raw_draw_if_possible,
                                   uint32_t client_id)
    : ImageContext(resource),
      maybe_concurrent_reads_(maybe_concurrent_reads),
      raw_draw_if_possible_(raw_draw_if_possible) {}

ImageContextImpl::ImageContextImpl(const gpu::Mailbox& mailbox,
                                   const gfx::Size& size,
                                   SharedImageFormat format,
                                   sk_sp<SkColorSpace> color_space)
    : ImageContext(mailbox,
                   gpu::SyncToken(),
                   /*texture_target=*/GL_TEXTURE_2D,
                   size,
                   format,
                   color_space,
                   /*origin=*/kTopLeft_GrSurfaceOrigin),
      is_for_render_pass_(true) {}

ImageContextImpl::~ImageContextImpl() {
  DeleteFallbackTextures();
}

void ImageContextImpl::OnContextLost() {
  if (texture_passthrough_) {
    texture_passthrough_->MarkContextLost();
    texture_passthrough_.reset();
  }

  if (representation_) {
    representation_->OnContextLost();
    representation_scoped_read_access_.reset();
    representation_.reset();
  }

  if (fallback_context_state_) {
    fallback_context_state_ = nullptr;
    fallback_textures_.clear();
    graphite_fallback_textures_.clear();
  }
}

void ImageContextImpl::SetPromiseImageTextures(
    std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures) {
  owned_promise_image_textures_ = std::move(promise_image_textures);
  promise_image_textures_.clear();
  for (auto& owned_texture : owned_promise_image_textures_) {
    promise_image_textures_.push_back(owned_texture.get());
  }
}

void ImageContextImpl::DeleteFallbackTextures() {
  if (fallback_context_state_) {
    if (fallback_context_state_->gr_context()) {
      CHECK(graphite_fallback_textures_.empty());
      for (auto& fallback_texture : fallback_textures_) {
        gpu::DeleteGrBackendTexture(fallback_context_state_, &fallback_texture);
      }
    } else {
      CHECK(fallback_context_state_->gpu_main_graphite_recorder());
      CHECK(fallback_textures_.empty());
      for (auto& fallback_texture : graphite_fallback_textures_) {
        fallback_context_state_->gpu_main_graphite_recorder()
            ->deleteBackendTexture(fallback_texture);
      }
    }
  }
  fallback_context_state_ = nullptr;
  fallback_textures_.clear();
  graphite_fallback_textures_.clear();
}

void ImageContextImpl::CreateFallbackImage(
    gpu::SharedContextState* context_state) {
  const int num_planes = format().NumberOfPlanes();
  TRACE_EVENT_BEGIN("viz", "ImageContextImpl::CreateFallbackImage");

  CreateFallbackImageResult result = CreateFallbackImageResult::kSuccess;
  absl::Cleanup record_results = [&result] {
    base::UmaHistogramEnumeration("Viz.CreateFallbackImageResult", result);
    TRACE_EVENT_END("viz", "result", CreateFallbackImageResultToString(result));
  };

  if (format().PrefersExternalSampler()) {
    // Skia can't allocate a fallback texture since the original texture was
    // externally allocated.
    result = CreateFallbackImageResult::kFailedPrefersExternalSampler;
    return;
  }

  if (context_state->graphite_shared_context()) {
    if (graphite_ycbcr_info_mismatch_) {
      // It is not possible to allocate a fallback texture if the failure was
      // due to a mismatch in YCBCr info between the promise image and the
      // fulfillment texture.
      result = CreateFallbackImageResult::kFailedYcbcrMismatch;
      return;
    }

    const auto& tex_infos = texture_infos();
    if (tex_infos.size() != static_cast<size_t>(num_planes) ||
        std::ranges::any_of(tex_infos, [](const auto& tex_info) {
          return !tex_info.isValid();
        })) {
      DLOG(ERROR) << "Invalid Graphite texture infos for format: "
                  << format().ToString();
      result = CreateFallbackImageResult::kFailedInvalidTextureInfo;
      return;
    }

#if BUILDFLAG(SKIA_USE_DAWN)
    skgpu::graphite::DawnTextureInfo dawn_info;
    bool success = skgpu::graphite::TextureInfos::GetDawnTextureInfo(
        tex_infos[0], &dawn_info);
    if (success && dawn_info.fFormat == wgpu::TextureFormat::External) {
      // Skia can't allocate a fallback texture since the original texture was
      // externally allocated.
      result = CreateFallbackImageResult::kFailedExternalTexture;
      return;
    }
#endif

    DCHECK(!fallback_context_state_);
    fallback_context_state_ = context_state;
    for (int plane_index = 0; plane_index < num_planes; plane_index++) {
      SkISize sk_size =
          gfx::SizeToSkISize(format().GetPlaneSize(plane_index, size()));
      auto tex_info =
          gpu::FallbackGraphiteBackendTextureInfo(tex_infos[plane_index]);
      auto texture = fallback_context_state_->gpu_main_graphite_recorder()
                         ->createBackendTexture(sk_size, tex_info);
      if (!texture.isValid()) {
        DLOG(ERROR) << "Failed to create fallback graphite backend texture";
        DeleteFallbackTextures();
        result = CreateFallbackImageResult::kFailedCreateTexture;
        return;
      }
      graphite_fallback_textures_.push_back(texture);
    }
    graphite_textures_ = graphite_fallback_textures_;

    for (int plane_index = 0; plane_index < num_planes; plane_index++) {
      SkColorType color_type = ToClosestSkColorType(format(), plane_index);
      auto sk_surface = SkSurfaces::WrapBackendTexture(
          fallback_context_state_->gpu_main_graphite_recorder(),
          graphite_fallback_textures_[plane_index], color_type, color_space(),
          /*props=*/nullptr);
      CHECK(sk_surface);
      sk_surface->getCanvas()->clear(
          GetFallbackColorForPlane(format(), plane_index));
    }

    // Snap and insert recording for GPU work needed for clearing the surface.
    // If this is not done, the fallback image may be used in a Submit after
    // destruction if it waits too long for snap to happen during rasterization
    // on GPU main thread.
    auto recording =
        fallback_context_state_->gpu_main_graphite_recorder()->snap();
    if (recording) {
      skgpu::graphite::InsertRecordingInfo info = {};
      info.fRecording = recording.get();
      bool insert_success =
          fallback_context_state_->graphite_shared_context()->insertRecording(
              info);
      if (!insert_success) {
        DLOG(ERROR) << "Failed to insert recording";
      }
    }
    return;
  }

  // We can't allocate a fallback texture as the original texture was externally
  // allocated. Skia will skip drawing a null GrPromiseImageTexture, do nothing
  // and leave it null.
  const auto& formats = backend_formats();
  if (formats.empty() || formats[0].textureType() == GrTextureType::kExternal) {
    result = CreateFallbackImageResult::kFailedExternalTexture;
    return;
  }

  DCHECK(!fallback_context_state_);
  fallback_context_state_ = context_state;

  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures;
  for (int plane_index = 0; plane_index < num_planes; plane_index++) {
    DCHECK_NE(formats[plane_index].textureType(), GrTextureType::kExternal);
    auto plane_size = format().GetPlaneSize(plane_index, size());
    auto fallback_texture =
        fallback_context_state_->gr_context()->createBackendTexture(
            plane_size.width(), plane_size.height(), formats[plane_index],
            GetFallbackColorForPlane(format(), plane_index),
            skgpu::Mipmapped::kNo, GrRenderable::kYes);

    if (!fallback_texture.isValid()) {
      DeleteFallbackTextures();
      DLOG(ERROR) << "Could not create backend texture.";
      result = CreateFallbackImageResult::kFailedCreateTexture;
      return;
    }
    auto promise_texture = GrPromiseImageTexture::Make(fallback_texture);
    promise_textures.push_back(std::move(promise_texture));
    fallback_textures_.push_back(fallback_texture);
  }
  SetPromiseImageTextures(promise_textures);
}

void ImageContextImpl::BeginAccessIfNecessary(
    gpu::SharedContextState* context_state,
    gpu::SharedImageRepresentationFactory* representation_factory,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (representation_raster_scoped_access_)
    return;

  if (!BeginAccessIfNecessaryInternal(context_state, representation_factory,
                                      begin_semaphores, end_semaphores)) {
    CreateFallbackImage(context_state);
  }
}

bool ImageContextImpl::BeginRasterAccess(
    gpu::SharedImageRepresentationFactory* representation_factory) {
  if (paint_op_buffer()) {
    DCHECK(raster_representation_);
    DCHECK(representation_raster_scoped_access_);
    return true;
  }

  auto raster = raw_draw_if_possible_
                    ? representation_factory->ProduceRaster(mailbox())
                    : nullptr;
  if (!raster)
    return false;

  auto scoped_access = raster->BeginScopedReadAccess();
  if (!scoped_access)
    return false;

  set_paint_op_buffer(scoped_access->paint_op_buffer());
  set_clear_color(scoped_access->clear_color());

  raster_representation_ = std::move(raster);
  representation_raster_scoped_access_ = std::move(scoped_access);

  return true;
}

bool ImageContextImpl::BeginAccessIfNecessaryInternal(
    gpu::SharedContextState* context_state,
    gpu::SharedImageRepresentationFactory* representation_factory,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  // Skip the context if it has been processed.
  if (representation_scoped_read_access_) {
    CHECK(owned_promise_image_textures_.empty());
    CHECK(!context_state->gr_context() || !promise_image_textures_.empty());
    CHECK(!context_state->graphite_shared_context() ||
          !graphite_textures_.empty());
    return true;
  }

  // promise_image_textures_ are not empty here, it means we are using a
  // fallback image.
  if (!promise_image_textures_.empty()) {
    CHECK_EQ(promise_image_textures_.size(),
             owned_promise_image_textures_.size());
    return true;
  }

  if (!graphite_textures_.empty()) {
    CHECK_EQ(graphite_textures_.size(), graphite_fallback_textures_.size());
    return true;
  }

  if (!representation_) {
    auto representation =
        representation_factory->ProduceSkia(mailbox(), context_state);
    if (!representation) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "mailbox not found in SharedImageManager.";
      return false;
    }

    if (!(representation->usage().Has(gpu::SHARED_IMAGE_USAGE_DISPLAY_READ))) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "was not created with DISPLAY_READ usage.";
      return false;
    }

    if (representation->size() != size()) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "size does not match TransferableResource size: "
                  << representation->size().ToString() << " vs "
                  << size().ToString();
      return false;
    }

    representation_ = std::move(representation);
  }

  representation_scoped_read_access_ =
      representation_->BeginScopedReadAccess(begin_semaphores, end_semaphores);
  if (!representation_scoped_read_access_) {
    representation_ = nullptr;
    DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                   "begin read access failed..";
    return false;
  }

  // Only one promise texture for external sampler case.
  int num_planes =
      format().PrefersExternalSampler() ? 1 : format().NumberOfPlanes();
  if (context_state->graphite_shared_context()) {
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(SKIA_USE_DAWN)
    // In the case of video decoding, it is possible for there to be a mismatch
    // between the YCbCr info passed to Viz at the time of creating the promise
    // texture and that computed at the time of fulfilling the promise texture.
    // Detect such mismatches and error out, as Skia/Dawn will raise errors.
    graphite_ycbcr_info_mismatch_ = false;

    skgpu::graphite::DawnTextureInfo fulfillment_texture_info;
    CHECK(skgpu::graphite::TextureInfos::GetDawnTextureInfo(
        representation_scoped_read_access_->graphite_texture(0).info(),
        &fulfillment_texture_info));

    wgpu::YCbCrVkDescriptor promise_texture_ycbcr_desc = {};
    if (ycbcr_info()) {
      promise_texture_ycbcr_desc =
          gpu::ToDawnYCbCrVkDescriptor(ycbcr_info().value());
    }
    wgpu::YCbCrVkDescriptor fulfillment_texture_ycbcr_desc =
        fulfillment_texture_info.fYcbcrVkDescriptor;

    if (!DawnYCbCrVkDescriptorsAreCompatible(promise_texture_ycbcr_desc,
                                             fulfillment_texture_ycbcr_desc)) {
      graphite_ycbcr_info_mismatch_ = true;
      representation_scoped_read_access_.reset();
      return false;
    }
#endif

    for (int plane_index = 0; plane_index < num_planes; plane_index++) {
      graphite_textures_.push_back(
          representation_scoped_read_access_->graphite_texture(plane_index));
    }
  } else {
    CHECK(context_state->gr_context());
    for (int plane_index = 0; plane_index < num_planes; plane_index++) {
      promise_image_textures_.push_back(
          representation_scoped_read_access_->promise_image_texture(
              plane_index));
    }
  }

  return true;
}

void ImageContextImpl::EndAccessIfNecessary() {
  if (paint_op_buffer()) {
    DCHECK(!representation_scoped_read_access_);
    return;
  }

  if (!representation_scoped_read_access_)
    return;

  // Avoid unnecessary read access churn for representations that
  // support multiple readers.
  if (representation_->SupportsMultipleConcurrentReadAccess() &&
      !is_for_render_pass_) {
    return;
  }

  representation_scoped_read_access_.reset();
  promise_image_textures_.clear();
  graphite_textures_.clear();
}

}  // namespace viz
