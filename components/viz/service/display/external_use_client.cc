// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/external_use_client.h"

#include "base/check.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gl/gl_bindings.h"

namespace viz {

ExternalUseClient::ImageContext::ImageContext(
    const gpu::Mailbox& mailbox,
    const gfx::Size& size,
    SharedImageFormat format,
    const gfx::ColorSpace& color_space)
    : mailbox_(mailbox),
      texture_target_(GL_TEXTURE_2D),
      size_(size),
      format_(format),
      color_space_(std::move(color_space)),
      origin_(kTopLeft_GrSurfaceOrigin) {}

ExternalUseClient::ImageContext::ImageContext(
    const TransferableResource& resource)
    : mailbox_(resource.mailbox()),
      sync_token_(resource.sync_token()),
      texture_target_(resource.texture_target()),
      size_(resource.GetSize()),
      format_(resource.GetFormat()),
      color_space_(resource.GetColorSpace()),
      origin_(resource.GetOrigin()),
      resource_source_(resource.resource_source) {
#if BUILDFLAG(IS_ANDROID)
  ycbcr_info_ = resource.ycbcr_info;
#endif
}

ExternalUseClient::ImageContext::~ImageContext() = default;

sk_sp<SkColorSpace> ExternalUseClient::ImageContext::GetSkColorSpace() const {
  // SkColorSpace covers only RGB portion of the gfx::ColorSpace, YUV
  // portion is handled via SkYuvColorSpace at places where we create YUV
  // images.
  return color_space_.GetAsFullRangeRGB().ToSkColorSpace();
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
