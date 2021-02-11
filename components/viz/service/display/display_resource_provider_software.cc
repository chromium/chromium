// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_software.h"

#include "components/viz/common/resources/resource_format_utils.h"

namespace viz {

DisplayResourceProviderSoftware::DisplayResourceProviderSoftware(
    SharedBitmapManager* shared_bitmap_manager)
    : DisplayResourceProvider(DisplayResourceProvider::kSoftware,
                              /*compositor_context_provider=*/nullptr,
                              shared_bitmap_manager,
                              /*enable_shared_images=*/true) {}

void DisplayResourceProviderSoftware::PopulateSkBitmapWithResource(
    SkBitmap* sk_bitmap,
    const ChildResource* resource) {
  DCHECK(IsBitmapFormatSupported(resource->transferable.format));
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(resource->transferable.size.width(),
                                 resource->transferable.size.height());
  bool pixels_installed = sk_bitmap->installPixels(
      info, resource->shared_bitmap->pixels(), info.minRowBytes());
  DCHECK(pixels_installed);
}

DisplayResourceProviderSoftware::ScopedReadLockSkImage::ScopedReadLockSkImage(
    DisplayResourceProviderSoftware* resource_provider,
    ResourceId resource_id,
    SkAlphaType alpha_type,
    GrSurfaceOrigin origin)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const ChildResource* resource =
      resource_provider->LockForRead(resource_id, false /* overlay_only */);
  DCHECK(resource);
  DCHECK(!resource->is_gpu_resource_type());

  // Use cached SkImage if possible.
  auto it = resource_provider_->resource_sk_images_.find(resource_id);
  if (it != resource_provider_->resource_sk_images_.end()) {
    sk_image_ = it->second;
    return;
  }

  if (!resource->shared_bitmap) {
    // If a CompositorFrameSink is destroyed, it destroys all SharedBitmapIds
    // that it registered. In this case, a CompositorFrame can be drawn with
    // SharedBitmapIds that are not known in the viz service. As well, a
    // misbehaved client can use SharedBitampIds that it did not report to
    // the service. Then the |shared_bitmap| will be null, and this read lock
    // will not be valid. Software-compositing users of this read lock must
    // check for valid() to deal with this scenario.
    sk_image_ = nullptr;
    return;
  }

  DCHECK(origin == kTopLeft_GrSurfaceOrigin);
  SkBitmap sk_bitmap;
  resource_provider->PopulateSkBitmapWithResource(&sk_bitmap, resource);
  sk_bitmap.setImmutable();
  sk_image_ = SkImage::MakeFromBitmap(sk_bitmap);
  resource_provider_->resource_sk_images_[resource_id] = sk_image_;
}

DisplayResourceProviderSoftware::ScopedReadLockSkImage::
    ~ScopedReadLockSkImage() {
  resource_provider_->UnlockForRead(resource_id_, false /* overlay_only */);
}

}  // namespace viz
