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
    ResourceFormat resource_format,
    const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space)
    : mailbox_holder_(mailbox_holder),
      size_(size),
      resource_format_(resource_format),
      color_space_(std::move(color_space)),
      ycbcr_info_(ycbcr_info) {}

ExternalUseClient::ImageContext::~ImageContext() = default;

sk_sp<SkColorSpace> ExternalUseClient::ImageContext::color_space() const {
  return color_space_;
}

void ExternalUseClient::ImageContext::OnContextLost() {
  NOTREACHED();
}

void ExternalUseClient::ImageContext::SetImage(sk_sp<SkImage> image,
                                               GrBackendFormat backend_format) {
  DCHECK(!image_);
  image_ = std::move(image);
  backend_format_ = backend_format;
}

}  // namespace viz
