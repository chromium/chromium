// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IMAGE_CONTEXT_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IMAGE_CONTEXT_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/service/display/external_use_client.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/geometry/size.h"

class SkColorSpace;
class SkPromiseImageTexture;

namespace gpu {
class MailboxManager;
class SharedContextState;
class SharedImageRepresentationFactory;
class TextureBase;
}  // namespace gpu

namespace viz {

// ImageContext can be accessed by compositor and GPU thread. It is a complete
// enough implementation for use in tests by FakeSkiaOutputSurface.
//
// ImageContextImpl adds functionality for use by SkiaOutputSurfaceImpl and
// SkiaOutputSurfaceImplOnGpu. {Begin,End}Access is called from the GPU thread.
class ImageContextImpl final : public ExternalUseClient::ImageContext {
 public:
  ImageContextImpl(const gpu::MailboxHolder& mailbox_holder,
                   const gfx::Size& size,
                   ResourceFormat resource_format,
                   const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
                   sk_sp<SkColorSpace> color_space);

  // TODO(https://crbug.com/991659): The use of ImageContext for
  // SkiaOutputSurfaceImplOnGpu::OffscreenSurface can be factored out. This
  // would make ImageContextImpl cleaner and handling of render passes less
  // confusing.
  ImageContextImpl(RenderPassId render_pass_id,
                   const gfx::Size& size,
                   ResourceFormat resource_format,
                   bool mipmap,
                   sk_sp<SkColorSpace> color_space);
  ~ImageContextImpl() final;

  void OnContextLost() final;

  RenderPassId render_pass_id() const { return render_pass_id_; }
  GrMipMapped mipmap() const { return mipmap_; }

  void set_promise_image_texture(
      sk_sp<SkPromiseImageTexture> promise_image_texture) {
    owned_promise_image_texture_ = std::move(promise_image_texture);
    promise_image_texture_ = owned_promise_image_texture_.get();
  }
  SkPromiseImageTexture* promise_image_texture() const {
    return promise_image_texture_;
  }

  void BeginAccessIfNecessary(
      gpu::SharedContextState* context_state,
      gpu::SharedImageRepresentationFactory* representation_factory,
      gpu::MailboxManager* mailbox_manager,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);
  void EndAccessIfNecessary();

 private:
  void CreateFallbackImage(gpu::SharedContextState* context_state);
  bool BeginAccessIfNecessaryForSharedImage(
      gpu::SharedContextState* context_state,
      gpu::SharedImageRepresentationFactory* representation_factory,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);

  // Returns true if |texture_base| is a gles2::Texture and all necessary
  // operations completed successfully. In this case, |*size| is the size of
  // of level 0.
  bool BindOrCopyTextureIfNecessary(gpu::TextureBase* texture_base,
                                    gfx::Size* size);

  const RenderPassId render_pass_id_ = 0;
  const GrMipMapped mipmap_ = GrMipMapped::kNo;

  // Fallback in case we cannot produce a |representation_|.
  gpu::SharedContextState* fallback_context_state_ = nullptr;
  GrBackendTexture fallback_texture_;

  std::unique_ptr<gpu::SharedImageRepresentationSkia> representation_;

  // For scoped read accessing |representation|. It is only accessed on GPU
  // thread.
  base::Optional<gpu::SharedImageRepresentationSkia::ScopedReadAccess>
      representation_scoped_read_access_;

  // For holding SkPromiseImageTexture create from |fallback_texture| or legacy
  // mailbox.
  sk_sp<SkPromiseImageTexture> owned_promise_image_texture_;

  // The |promise_image_texture| is used for fulfilling the promise image. It is
  // used on GPU thread.
  SkPromiseImageTexture* promise_image_texture_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ImageContextImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IMAGE_CONTEXT_IMPL_H_
