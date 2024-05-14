// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/external_use_client.h"
#include "base/check.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace viz {

ExternalUseClient::ImageContext::ImageContext(
    const gpu::MailboxHolder& mailbox_holder,
    const gfx::Size& size,
    SharedImageFormat format,
    const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space)
    : mailbox_holder_(mailbox_holder),
      size_(size),
      format_(format),
      color_space_(std::move(color_space)),
      ycbcr_info_(ycbcr_info) {}

ExternalUseClient::ImageContext::~ImageContext() = default;

sk_sp<SkColorSpace> ExternalUseClient::ImageContext::color_space() const {
  return color_space_;
}

void ExternalUseClient::ImageContext::OnContextLost() {
  NOTREACHED_IN_MIGRATION();
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
