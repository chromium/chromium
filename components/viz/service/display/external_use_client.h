// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EXTERNAL_USE_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EXTERNAL_USE_CLIENT_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/geometry/size.h"

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
                 ResourceFormat resource_format,
                 const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
                 sk_sp<SkColorSpace> color_space);
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
    ResourceFormat resource_format() const { return resource_format_; }
    sk_sp<SkColorSpace> color_space() const { return color_space_; }

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

    base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info() { return ycbcr_info_; }

    bool has_image() { return !!image_; }
    sk_sp<SkImage> image() { return image_; }
    void SetImage(sk_sp<SkImage> image, GrBackendFormat backend_format);
    void clear_image() { image_.reset(); }
    const GrBackendFormat& backend_format() { return backend_format_; }

   private:
    gpu::MailboxHolder mailbox_holder_;

    const gfx::Size size_;
    const ResourceFormat resource_format_;
    const sk_sp<SkColorSpace> color_space_;

    SkAlphaType alpha_type_ = kPremul_SkAlphaType;
    GrSurfaceOrigin origin_ = kTopLeft_GrSurfaceOrigin;

    // Sampler conversion information which is used in vulkan context for
    // android video.
    base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info_;

    // The promise image which is used on display thread.
    sk_sp<SkImage> image_;
    GrBackendFormat backend_format_;

    DISALLOW_COPY_AND_ASSIGN(ImageContext);
  };

  virtual std::unique_ptr<ImageContext> CreateImageContext(
      const gpu::MailboxHolder& holder,
      const gfx::Size& size,
      ResourceFormat format,
      const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
      sk_sp<SkColorSpace> color_space) = 0;

  virtual void ReleaseImageContexts(
      std::vector<std::unique_ptr<ImageContext>> image_contexts) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EXTERNAL_USE_CLIENT_H_
