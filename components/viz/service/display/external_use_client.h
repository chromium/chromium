// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EXTERNAL_USE_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EXTERNAL_USE_CLIENT_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/graphite/TextureInfo.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class PaintOpBuffer;
}

namespace viz {

// DisplayResourceProvider takes ownership
// of ImageContext from ExternalUseClient when it calls CreateImage.
// DisplayResourceProvider returns ownership when it calls ReleaseImageContexts.
// An ExternalUseClient will only ever release an ImageContext that it has
// created.
class VIZ_SERVICE_EXPORT ExternalUseClient {
 public:
  class VIZ_SERVICE_EXPORT ImageContext {
   public:
    ImageContext(const gpu::MailboxHolder& mailbox_holder,
                 const gfx::Size& size,
                 SharedImageFormat format,
                 const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
                 sk_sp<SkColorSpace> color_space);

    ImageContext(const ImageContext&) = delete;
    ImageContext& operator=(const ImageContext&) = delete;

    virtual ~ImageContext();
    virtual void OnContextLost();

    //
    // Thread safety is guaranteed by these invariants: (a) only the compositor
    // thread modifies ImageContext, (b) ImageContext is not modified after
    // |image| is set, and (c) GPU thread only reads ImageContext after |image|
    // is set.
    //
    const gpu::MailboxHolder& mailbox_holder() const { return mailbox_holder_; }
    gpu::MailboxHolder* mutable_mailbox_holder() { return &mailbox_holder_; }
    const gfx::Size& size() const { return size_; }
    SharedImageFormat format() const { return format_; }
    sk_sp<SkColorSpace> color_space() const;

    SkAlphaType alpha_type() const { return alpha_type_; }
    void set_alpha_type(SkAlphaType alpha_type) {
      DCHECK(!image_);
      alpha_type_ = alpha_type;
    }

    GrSurfaceOrigin origin() const { return origin_; }
    void set_origin(GrSurfaceOrigin origin) {
      DCHECK(!image_);
      origin_ = origin;
    }

    std::optional<gpu::VulkanYCbCrInfo> ycbcr_info() { return ycbcr_info_; }

    bool has_image() { return !!image_; }
    sk_sp<SkImage> image() { return image_; }
    void SetImage(sk_sp<SkImage> image,
                  std::vector<GrBackendFormat> backend_formats);
    void SetImage(sk_sp<SkImage> image,
                  std::vector<skgpu::graphite::TextureInfo> texture_infos);
    void clear_image() { image_.reset(); }
    const std::vector<GrBackendFormat>& backend_formats() {
      return backend_formats_;
    }
    const std::vector<skgpu::graphite::TextureInfo>& texture_infos() {
      return texture_infos_;
    }

    const cc::PaintOpBuffer* paint_op_buffer() const {
      return paint_op_buffer_;
    }
    void set_paint_op_buffer(const cc::PaintOpBuffer* buffer) {
      paint_op_buffer_ = buffer;
    }
    const std::optional<SkColor4f>& clear_color() const { return clear_color_; }
    void set_clear_color(const std::optional<SkColor4f>& color) {
      clear_color_ = color;
    }

   private:
    gpu::MailboxHolder mailbox_holder_;

    const gfx::Size size_;
    const SharedImageFormat format_;
    const sk_sp<SkColorSpace> color_space_;

    SkAlphaType alpha_type_ = kPremul_SkAlphaType;
    GrSurfaceOrigin origin_ = kTopLeft_GrSurfaceOrigin;

    // Sampler conversion information which is used in vulkan context for
    // android video.
    std::optional<gpu::VulkanYCbCrInfo> ycbcr_info_;

    // The promise image which is used on display thread.
    sk_sp<SkImage> image_;
    std::vector<GrBackendFormat> backend_formats_;
    std::vector<skgpu::graphite::TextureInfo> texture_infos_;
    raw_ptr<const cc::PaintOpBuffer> paint_op_buffer_ = nullptr;
    std::optional<SkColor4f> clear_color_;
  };

  // If |maybe_concurrent_reads| is true then there can be concurrent reads to
  // the texture that modify GL texture parameters.
  virtual std::unique_ptr<ImageContext> CreateImageContext(
      const gpu::MailboxHolder& holder,
      const gfx::Size& size,
      SharedImageFormat format,
      bool maybe_concurrent_reads,
      const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
      sk_sp<SkColorSpace> color_space,
      bool raw_draw_if_possible) = 0;

  virtual gpu::SyncToken ReleaseImageContexts(
      std::vector<std::unique_ptr<ImageContext>> image_contexts) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EXTERNAL_USE_CLIENT_H_
