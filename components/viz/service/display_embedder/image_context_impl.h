// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IMAGE_CONTEXT_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IMAGE_CONTEXT_IMPL_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/display/external_use_client.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/geometry/size.h"

class SkColorSpace;

namespace gpu {
class SharedContextState;
class SharedImageRepresentationFactory;
namespace gles2 {
class TexturePassthrough;
}
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
                   SharedImageFormat format,
                   bool maybe_concurrent_reads,
                   const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
                   sk_sp<SkColorSpace> color_space,
                   bool is_for_render_pass,
                   bool raw_draw_if_possible = false);

  ImageContextImpl(const ImageContextImpl&) = delete;
  ImageContextImpl& operator=(const ImageContextImpl&) = delete;

  ~ImageContextImpl() final;

  void OnContextLost() final;

  // Returns true if there might be concurrent reads to the backing texture.
  bool maybe_concurrent_reads() const { return maybe_concurrent_reads_; }

  // Return the vector of promise image textures.
  const std::vector<raw_ptr<GrPromiseImageTexture>>& promise_image_textures()
      const {
    return promise_image_textures_;
  }
  const std::vector<skgpu::graphite::BackendTexture>& graphite_textures()
      const {
    return graphite_textures_;
  }
  bool HasAccessEndState() const {
    return representation_scoped_read_access_
               ? representation_scoped_read_access_->HasBackendSurfaceEndState()
               : false;
  }
  void ApplyAccessEndState() const {
    if (representation_scoped_read_access_) {
      representation_scoped_read_access_->ApplyBackendSurfaceEndState();
    }
  }

  void SetPromiseImageTextures(
      std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures);

  void BeginAccessIfNecessary(
      gpu::SharedContextState* context_state,
      gpu::SharedImageRepresentationFactory* representation_factory,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);
  bool BeginRasterAccess(
      gpu::SharedImageRepresentationFactory* representation_factory);
  void EndAccessIfNecessary();

 private:
  void DeleteFallbackTextures();

  // Creates a solid color fallback image that can be substituted for the
  // original image. Note that this may fail if it's not possible to allocate a
  // fallback image, for example if the original image was externally allocated.
  // In this case the promise image fulfillment will fail and skia will abort
  // drawing the entire render pass, so we rely on this being a transient state.
  void CreateFallbackImage(gpu::SharedContextState* context_state);
  bool BeginAccessIfNecessaryInternal(
      gpu::SharedContextState* context_state,
      gpu::SharedImageRepresentationFactory* representation_factory,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);

  const bool maybe_concurrent_reads_ = false;
  // Indicates that this will be used to refer to allocations that originate
  // from the renderer.
  const bool is_for_render_pass_ = false;
  const bool raw_draw_if_possible_ = false;

  // Fallback in case we cannot produce a |representation_|.
  raw_ptr<gpu::SharedContextState> fallback_context_state_ = nullptr;
  std::vector<GrBackendTexture> fallback_textures_;
  // Fallback textures used for fulfilling Graphite promise images. Owned by the
  // ImageContextImpl and must be destroyed on ImageContextImpl destruction.
  std::vector<skgpu::graphite::BackendTexture> graphite_fallback_textures_;

  // Only one of the follow should be non-null at the same time.
  scoped_refptr<gpu::gles2::TexturePassthrough> texture_passthrough_;
  std::unique_ptr<gpu::SkiaImageRepresentation> representation_;
  std::unique_ptr<gpu::RasterImageRepresentation> raster_representation_;

  // For scoped read accessing |representation|. It is only accessed on GPU
  // thread.
  std::unique_ptr<gpu::SkiaImageRepresentation::ScopedReadAccess>
      representation_scoped_read_access_;
  std::unique_ptr<gpu::RasterImageRepresentation::ScopedReadAccess>
      representation_raster_scoped_access_;

  // For holding GrPromiseImageTexture create from |fallback_texture| or legacy
  // mailboxes.
  std::vector<sk_sp<GrPromiseImageTexture>> owned_promise_image_textures_;

  // The |promise_image_textures| are used for fulfilling the promise images.
  // They are used on GPU thread.
  std::vector<raw_ptr<GrPromiseImageTexture>> promise_image_textures_;

  // Graphite backend textures used for fulfilling Graphite promise images.
  // Owned by the shared image representation / scoped access.
  std::vector<skgpu::graphite::BackendTexture> graphite_textures_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IMAGE_CONTEXT_IMPL_H_
