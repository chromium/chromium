// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/image_context_impl.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrContextThreadSafeProxy.h"
#include "ui/gl/gl_image.h"

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
    case viz::SharedImageFormat::PlaneConfig::kY_V_U:
      return plane_index == 0 ? SkColors::kWhite : SkColors::kGray;
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
    const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space,
    bool allow_keeping_read_access,
    bool raw_draw_if_possible)
    : ImageContext(mailbox_holder, size, format, ycbcr_info, color_space),
      maybe_concurrent_reads_(maybe_concurrent_reads),
      allow_keeping_read_access_(allow_keeping_read_access),
      raw_draw_if_possible_(raw_draw_if_possible) {}

ImageContextImpl::~ImageContextImpl() {
  DeleteFallbackGrBackendTextures();
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
  }
}

void ImageContextImpl::SetPromiseImageTextures(
    std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures) {
  owned_promise_image_textures_ = std::move(promise_image_textures);
  promise_image_textures_.clear();
  for (auto& owned_texture : owned_promise_image_textures_) {
    promise_image_textures_.push_back(owned_texture.get());
  }
}

void ImageContextImpl::DeleteFallbackGrBackendTextures() {
  if (fallback_context_state_) {
    for (auto& fallback_texture : fallback_textures_)
      gpu::DeleteGrBackendTexture(fallback_context_state_, &fallback_texture);
  }
  fallback_context_state_ = nullptr;
  fallback_textures_.clear();
}

void ImageContextImpl::CreateFallbackImage(
    gpu::SharedContextState* context_state) {
  // We can't allocate a fallback texture as the original texture was externally
  // allocated. Skia will skip drawing a null SkPromiseImageTexture, do nothing
  // and leave it null.
  const auto& formats = backend_formats();
  if (formats.empty() || formats[0].textureType() == GrTextureType::kExternal)
    return;

  DCHECK(!fallback_context_state_);
  fallback_context_state_ = context_state;

  std::vector<sk_sp<SkPromiseImageTexture>> promise_textures;
  for (int plane_index = 0; plane_index < format().NumberOfPlanes();
       plane_index++) {
    DCHECK_NE(formats[plane_index].textureType(), GrTextureType::kExternal);
    auto fallback_texture =
        fallback_context_state_->gr_context()->createBackendTexture(
            size().width(), size().height(), formats[plane_index],
            GetFallbackColorForPlane(format(), plane_index), GrMipMapped::kNo,
            GrRenderable::kYes);

    if (!fallback_texture.isValid()) {
      DeleteFallbackGrBackendTextures();
      DLOG(ERROR) << "Could not create backend texture.";
      return;
    }
    auto promise_texture = SkPromiseImageTexture::Make(fallback_texture);
    promise_textures.push_back(std::move(promise_texture));
    fallback_textures_.push_back(fallback_texture);
  }
  SetPromiseImageTextures(promise_textures);
}

void ImageContextImpl::BeginAccessIfNecessary(
    gpu::SharedContextState* context_state,
    gpu::SharedImageRepresentationFactory* representation_factory,
    gpu::MailboxManager* mailbox_manager,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (representation_raster_scoped_access_)
    return;

  // Prepare for accessing shared image.
  if (mailbox_holder().mailbox.IsSharedImage()) {
    if (!BeginAccessIfNecessaryForSharedImage(
            context_state, representation_factory, begin_semaphores,
            end_semaphores)) {
      CreateFallbackImage(context_state);
    }
    return;
  }

  // Prepare for accessing legacy mailbox.
  // The promise images have been fulfilled once, so we don't need to do
  // anything.
  if (!promise_image_textures_.empty())
    return;

  if (!context_state->GrContextIsGL()) {
    // Probably this texture is created with wrong interface
    // (GLES2Interface).
    DLOG(ERROR) << "Failed to fulfill the promise texture whose backend is not "
                   "compatible with vulkan.";
    CreateFallbackImage(context_state);
    return;
  }

  auto* texture_base =
      mailbox_manager->ConsumeTexture(mailbox_holder().mailbox);
  if (!texture_base) {
    DLOG(ERROR) << "Failed to fulfill the promise texture.";
    CreateFallbackImage(context_state);
    return;
  }

  gfx::Size texture_size;
  if (BindOrCopyTextureIfNecessary(texture_base, &texture_size) &&
      texture_size != size()) {
    DLOG(ERROR) << "Failed to fulfill the promise texture - texture "
                   "size does not match TransferableResource size: "
                << texture_size.ToString() << " vs " << size().ToString();
    CreateFallbackImage(context_state);
    return;
  }

  // Legacy mailboxes support only single planar formats.
  DCHECK(format().is_single_plane());
  bool angle_rgbx_internal_format =
      context_state->feature_info()->feature_flags().angle_rgbx_internal_format;
  GrBackendTexture backend_texture;
  GLenum gl_storage_internal_format =
      gpu::TextureStorageFormat(format(), angle_rgbx_internal_format);
  gpu::GetGrBackendTexture(
      context_state->feature_info(), texture_base->target(), size(),
      texture_base->service_id(), gl_storage_internal_format,
      context_state->gr_context()->threadSafeProxy(), &backend_texture);
  if (!backend_texture.isValid()) {
    DLOG(ERROR) << "Failed to fulfill the promise texture.";
    CreateFallbackImage(context_state);
    return;
  }
  SetPromiseImageTextures({SkPromiseImageTexture::Make(backend_texture)});

  // Hold onto a reference to legacy GL textures while still in use, see
  // https://crbug.com/1118166 for why this is necessary.
  if (texture_base->GetType() == gpu::TextureBase::Type::kPassthrough) {
    texture_passthrough_ =
        gpu::gles2::TexturePassthrough::CheckedCast(texture_base);
  }
  // TODO(crbug.com/1118166): The case above handles textures with the
  // passthrough command decoder, verify if something is required for the
  // validating command decoder as well.
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

bool ImageContextImpl::BeginAccessIfNecessaryForSharedImage(
    gpu::SharedContextState* context_state,
    gpu::SharedImageRepresentationFactory* representation_factory,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  // Skip the context if it has been processed.
  if (representation_scoped_read_access_) {
    DCHECK(owned_promise_image_textures_.empty());
    DCHECK(!promise_image_textures_.empty());
    return true;
  }

  // promise_image_textures_ are not empty here, it means we are using a
  // fallback image.
  if (!promise_image_textures_.empty()) {
    DCHECK_EQ(promise_image_textures_.size(),
              owned_promise_image_textures_.size());
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

    if (!(representation->usage() & gpu::SHARED_IMAGE_USAGE_DISPLAY_READ)) {
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
  for (int plane_index = 0; plane_index < num_planes; plane_index++) {
    promise_image_textures_.push_back(
        representation_scoped_read_access_->promise_image_texture(plane_index));
  }

  return true;
}

bool ImageContextImpl::BindOrCopyTextureIfNecessary(
    gpu::TextureBase* texture_base,
    gfx::Size* size) {
  if (texture_base->GetType() != gpu::TextureBase::Type::kValidated)
    return false;
  // If a texture is validated and bound to an image, we may defer copying the
  // image to the texture until the texture is used. It is for implementing low
  // latency drawing (e.g. fast ink) and avoiding unnecessary texture copy. So
  // we need check the texture image state, and bind or copy the image to the
  // texture if necessary.
  auto* texture = gpu::gles2::Texture::CheckedCast(texture_base);
  gpu::gles2::Texture::ImageState image_state;
  auto* image = texture->GetLevelImage(GL_TEXTURE_2D, 0, &image_state);
  if (image && image_state == gpu::gles2::Texture::UNBOUND) {
    glBindTexture(texture_base->target(), texture_base->service_id());
    if (image->ShouldBindOrCopy() == gl::GLImage::BIND) {
      if (!image->BindTexImage(texture_base->target())) {
        LOG(ERROR) << "Failed to bind a gl image to texture.";
        return false;
      }
    } else {
      texture->SetLevelImageState(texture_base->target(), 0,
                                  gpu::gles2::Texture::COPIED);
      if (!image->CopyTexImage(texture_base->target())) {
        LOG(ERROR) << "Failed to copy a gl image to texture.";
        return false;
      }
    }
  }
  GLsizei temp_width, temp_height;
  texture->GetLevelSize(texture_base->target(), 0 /* level */, &temp_width,
                        &temp_height, nullptr /* depth */);
  *size = gfx::Size(temp_width, temp_height);
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
      allow_keeping_read_access_)
    return;

  representation_scoped_read_access_.reset();
  promise_image_textures_.clear();
}

}  // namespace viz
