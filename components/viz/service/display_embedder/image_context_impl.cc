// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/image_context_impl.h"

#include <utility>

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"

namespace viz {

ImageContextImpl::ImageContextImpl(
    const gpu::MailboxHolder& mailbox_holder,
    const gfx::Size& size,
    ResourceFormat resource_format,
    const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space)
    : ImageContext(mailbox_holder,
                   size,
                   resource_format,
                   ycbcr_info,
                   color_space) {}

ImageContextImpl::ImageContextImpl(RenderPassId render_pass_id,
                                   const gfx::Size& size,
                                   ResourceFormat resource_format,
                                   bool mipmap,
                                   sk_sp<SkColorSpace> color_space)
    : ImageContext(gpu::MailboxHolder(),
                   size,
                   resource_format,
                   /*ycbcr_info=*/base::nullopt,
                   std::move(color_space)),
      render_pass_id_(render_pass_id),
      mipmap_(mipmap ? GrMipMapped::kYes : GrMipMapped::kNo) {}

void ImageContextImpl::OnContextLost() {
  if (representation_) {
    representation_->OnContextLost();
    representation_ = nullptr;
  }
}

ImageContextImpl::~ImageContextImpl() {
  DCHECK(!representation_scoped_read_access_);

  if (fallback_context_state_)
    gpu::DeleteGrBackendTexture(fallback_context_state_, &fallback_texture_);
}

void ImageContextImpl::CreateFallbackImage(
    gpu::SharedContextState* context_state) {
  DCHECK(!fallback_context_state_);
  fallback_context_state_ = context_state;

  fallback_texture_ =
      fallback_context_state_->gr_context()->createBackendTexture(
          size().width(), size().height(), backend_format(),
#if DCHECK_IS_ON()
          SkColors::kRed,
#else
          SkColors::kWhite,
#endif
          GrMipMapped::kNo, GrRenderable::kYes);

  if (!fallback_texture_.isValid()) {
    DLOG(ERROR) << "Could not create backend texture.";
    return;
  }
  set_promise_image_texture(SkPromiseImageTexture::Make(fallback_texture_));
}

void ImageContextImpl::BeginAccessIfNecessary(
    gpu::SharedContextState* context_state,
    gpu::SharedImageRepresentationFactory* representation_factory,
    gpu::MailboxManager* mailbox_manager,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  // Prepare for accessing shared image.
  if (mailbox_holder().mailbox.IsSharedImage() &&
      BeginAccessIfNecessaryForSharedImage(context_state,
                                           representation_factory,
                                           begin_semaphores, end_semaphores)) {
    return;
  }

  // Prepare for accessing legacy mailbox.
  // The promise image has been fulfilled once, so we do need do anything.
  if (promise_image_texture_)
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
                   "size does not match TransferableResource size.";
    CreateFallbackImage(context_state);
    return;
  }

  GrBackendTexture backend_texture;
  gpu::GetGrBackendTexture(
      context_state->feature_info(), texture_base->target(), size(),
      texture_base->service_id(), resource_format(), &backend_texture);
  if (!backend_texture.isValid()) {
    DLOG(ERROR) << "Failed to fulfill the promise texture.";
    CreateFallbackImage(context_state);
    return;
  }
  set_promise_image_texture(SkPromiseImageTexture::Make(backend_texture));
}

bool ImageContextImpl::BeginAccessIfNecessaryForSharedImage(
    gpu::SharedContextState* context_state,
    gpu::SharedImageRepresentationFactory* representation_factory,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  // Skip the context if it has been processed.
  if (representation_scoped_read_access_) {
    DCHECK(!owned_promise_image_texture_);
    DCHECK(promise_image_texture_);
    return true;
  }

  // promise_image_texture_ is not null here, it means we are using a fallback
  // image.
  if (promise_image_texture_) {
    DCHECK(owned_promise_image_texture_);
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

    if (!(representation->usage() & gpu::SHARED_IMAGE_USAGE_DISPLAY)) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "was not created with display usage.";
      return false;
    }

    if (representation->size() != size()) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "size does not match TransferableResource size.";
      return false;
    }

    representation_ = std::move(representation);
  }

  representation_scoped_read_access_.emplace(representation_.get(),
                                             begin_semaphores, end_semaphores);
  if (!representation_scoped_read_access_->success()) {
    representation_scoped_read_access_.reset();
    representation_ = nullptr;
    DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                   "begin read access failed..";
    return false;
  }
  promise_image_texture_ =
      representation_scoped_read_access_->promise_image_texture();
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
  if (!representation_scoped_read_access_)
    return;
  representation_scoped_read_access_.reset();
  promise_image_texture_ = nullptr;
}

}  // namespace viz
