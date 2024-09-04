// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/image_context_impl.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrContextThreadSafeProxy.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnTypes.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

namespace {

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

ImageContextImpl::ImageContextImpl(
    const gpu::MailboxHolder& mailbox_holder,
    const gfx::Size& size,
    SharedImageFormat format,
    bool maybe_concurrent_reads,
    const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space,
    bool is_for_render_pass,
    bool raw_draw_if_possible)
    : ImageContext(mailbox_holder, size, format, ycbcr_info, color_space),
      maybe_concurrent_reads_(maybe_concurrent_reads),
      is_for_render_pass_(is_for_render_pass),
      raw_draw_if_possible_(raw_draw_if_possible) {}

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

  if (format().PrefersExternalSampler()) {
    // Skia can't allocate a fallback texture since the original texture was
    // externally allocated.
    return;
  }

  if (context_state->graphite_context()) {
    const auto& tex_infos = texture_infos();
    if (tex_infos.size() != static_cast<size_t>(num_planes) ||
        base::ranges::any_of(tex_infos, [](const auto& tex_info) {
          return !tex_info.isValid();
        })) {
      DLOG(ERROR) << "Invalid Graphite texture infos for format: "
                  << format().ToString();
      return;
    }

#if BUILDFLAG(SKIA_USE_DAWN)
    skgpu::graphite::DawnTextureInfo dawn_info;
    bool success = skgpu::graphite::TextureInfos::GetDawnTextureInfo(
        tex_infos[0], &dawn_info);
    if (success && dawn_info.fFormat == wgpu::TextureFormat::External) {
      // Skia can't allocate a fallback texture since the original texture was
      // externally allocated.
      return;
    }
#endif

    for (int plane_index = 0; plane_index < num_planes; plane_index++) {
      SkISize sk_size =
          gfx::SizeToSkISize(format().GetPlaneSize(plane_index, size()));
      auto tex_info =
          gpu::FallbackGraphiteBackendTextureInfo(tex_infos[plane_index]);
      graphite_fallback_textures_.push_back(
          context_state->gpu_main_graphite_recorder()->createBackendTexture(
              sk_size, tex_info));

      SkColorType color_type =
          ToClosestSkColorType(/*gpu_compositing=*/true, format(), plane_index);

      auto sk_surface = SkSurfaces::WrapBackendTexture(
          context_state->gpu_main_graphite_recorder(),
          graphite_fallback_textures_[plane_index], color_type, color_space(),
          /*props=*/nullptr);
      if (!sk_surface) {
        DLOG(ERROR) << "Failed to create fallback graphite backend texture";
        DeleteFallbackTextures();
        return;
      }
      sk_surface->getCanvas()->clear(
          GetFallbackColorForPlane(format(), plane_index));
    }
    graphite_textures_ = graphite_fallback_textures_;
    return;
  }

  // We can't allocate a fallback texture as the original texture was externally
  // allocated. Skia will skip drawing a null GrPromiseImageTexture, do nothing
  // and leave it null.
  const auto& formats = backend_formats();
  if (formats.empty() || formats[0].textureType() == GrTextureType::kExternal) {
    return;
  }

  DCHECK(!fallback_context_state_);
  fallback_context_state_ = context_state;

  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures;
  for (int plane_index = 0; plane_index < num_planes; plane_index++) {
    DCHECK_NE(formats[plane_index].textureType(), GrTextureType::kExternal);
    auto fallback_texture =
        fallback_context_state_->gr_context()->createBackendTexture(
            size().width(), size().height(), formats[plane_index],
            GetFallbackColorForPlane(format(), plane_index),
            skgpu::Mipmapped::kNo, GrRenderable::kYes);

    if (!fallback_texture.isValid()) {
      DeleteFallbackTextures();
      DLOG(ERROR) << "Could not create backend texture.";
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

  auto raster =
      raw_draw_if_possible_
          ? representation_factory->ProduceRaster(mailbox_holder().mailbox)
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
    CHECK(!context_state->graphite_context() || !graphite_textures_.empty());
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
    auto representation = representation_factory->ProduceSkia(
        mailbox_holder().mailbox, context_state);
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
  if (context_state->graphite_context()) {
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
