// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/external_use_client.h"
#include "base/check.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace viz {

ExternalUseClient::ImageContext::ImageContext(const gpu::Mailbox& mailbox,
                                              const gpu::SyncToken& sync_token,
                                              uint32_t texture_target,
                                              const gfx::Size& size,
                                              SharedImageFormat format,
                                              sk_sp<SkColorSpace> color_space,
                                              GrSurfaceOrigin origin)
    : mailbox_(mailbox),
      sync_token_(sync_token),
      texture_target_(texture_target),
      size_(size),
      format_(format),
      color_space_(std::move(color_space)),
      origin_(origin) {}

ExternalUseClient::ImageContext::ImageContext(
    const TransferableResource& resource)
    : mailbox_(resource.mailbox()),
      sync_token_(resource.sync_token()),
      texture_target_(resource.texture_target()),
      size_(resource.size),
      format_(resource.format),
      // SkColorSpace covers only RGB portion of the gfx::ColorSpace, YUV
      // portion is handled via SkYuvColorSpace at places where we create YUV
      // images.
      color_space_(resource.color_space.GetAsFullRangeRGB().ToSkColorSpace()),
      origin_(resource.origin),
      resource_source_(resource.resource_source),
      ycbcr_info_(resource.ycbcr_info) {}

ExternalUseClient::ImageContext::~ImageContext() = default;

sk_sp<SkColorSpace> ExternalUseClient::ImageContext::color_space() const {
  return color_space_;
}

void ExternalUseClient::ImageContext::OnContextLost() {
  NOTREACHED();
}

void ExternalUseClient::ImageContext::SetImage(
    sk_sp<SkImage> image,
    std::vector<GrBackendFormat> backend_formats) {
  CHECK(!image_);
  image_ = std::move(image);
  backend_formats_ = std::move(backend_formats);
}

void ExternalUseClient::ImageContext::SetImage(
    sk_sp<SkImage> image,
    std::vector<skgpu::graphite::TextureInfo> texture_infos) {
  CHECK(!image_);
  image_ = std::move(image);
  texture_infos_ = std::move(texture_infos);
}

}  // namespace viz
