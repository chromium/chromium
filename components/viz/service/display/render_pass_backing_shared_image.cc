// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/render_pass_backing_shared_image.h"

#include <utility>

#include "base/check.h"
#include "components/viz/service/display/skia_output_surface.h"

namespace viz {

RenderPassBackingSharedImage::RenderPassBackingSharedImage(
    SkiaOutputSurface* skia_output_surface,
    scoped_refptr<gpu::ClientSharedImage> shared_image)
    : skia_output_surface_(skia_output_surface),
      shared_image_(std::move(shared_image)) {
  CHECK(skia_output_surface_);
  CHECK(shared_image_);
}

RenderPassBackingSharedImage::RenderPassBackingSharedImage(
    RenderPassBackingSharedImage&& other)
    : skia_output_surface_(std::exchange(other.skia_output_surface_, nullptr)),
      shared_image_(std::move(other.shared_image_)) {}

RenderPassBackingSharedImage& RenderPassBackingSharedImage::operator=(
    RenderPassBackingSharedImage&& other) {
  if (this != &other) {
    Destroy();
    skia_output_surface_ = std::exchange(other.skia_output_surface_, nullptr);
    shared_image_ = std::move(other.shared_image_);
  }
  return *this;
}

RenderPassBackingSharedImage::~RenderPassBackingSharedImage() {
  Destroy();
}

const gpu::Mailbox& RenderPassBackingSharedImage::mailbox() const {
  return shared_image_->mailbox();
}

void RenderPassBackingSharedImage::Destroy() {
  if (SkiaOutputSurface* skia_output_surface =
          std::exchange(skia_output_surface_, nullptr)) {
    skia_output_surface->OnDestroySharedImage(shared_image_->mailbox());
    shared_image_.reset();
  } else {
    CHECK(!shared_image_);
  }
}

}  // namespace viz
