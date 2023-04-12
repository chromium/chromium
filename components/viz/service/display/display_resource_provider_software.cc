// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_software.h"

#include <memory>
#include <vector>

#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "third_party/skia/include/core/SkImage.h"

namespace viz {

DisplayResourceProviderSoftware::DisplayResourceProviderSoftware(
    SharedBitmapManager* shared_bitmap_manager)
    : DisplayResourceProvider(DisplayResourceProvider::kSoftware),
      shared_bitmap_manager_(shared_bitmap_manager) {
  DCHECK(shared_bitmap_manager);
}

DisplayResourceProviderSoftware::~DisplayResourceProviderSoftware() {
  Destroy();
}

const DisplayResourceProvider::ChildResource*
DisplayResourceProviderSoftware::LockForRead(ResourceId id) {
  ChildResource* resource = GetResource(id);

  DCHECK(!resource->is_gpu_resource_type());

  if (!resource->shared_bitmap) {
    const SharedBitmapId& shared_bitmap_id =
        resource->transferable.mailbox_holder.mailbox;
    std::unique_ptr<SharedBitmap> bitmap =
        shared_bitmap_manager_->GetSharedBitmapFromId(
            resource->transferable.size, resource->transferable.format,
            shared_bitmap_id);
    if (bitmap) {
      resource->shared_bitmap = std::move(bitmap);
      resource->shared_bitmap_tracing_guid =
          shared_bitmap_manager_->GetSharedBitmapTracingGUIDFromId(
              shared_bitmap_id);
    }
  }

  resource->lock_for_read_count++;
  return resource;
}

void DisplayResourceProviderSoftware::UnlockForRead(ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ChildResource* resource = GetResource(id);

  DCHECK(!resource->is_gpu_resource_type());
  DCHECK_GT(resource->lock_for_read_count, 0);
  resource->lock_for_read_count--;
  TryReleaseResource(id, resource);
}

std::vector<ReturnedResource>
DisplayResourceProviderSoftware::DeleteAndReturnUnusedResourcesToChildImpl(
    Child& child_info,
    DeleteStyle style,
    const std::vector<ResourceId>& unused) {
  std::vector<ReturnedResource> to_return;
  // Reserve enough space to avoid re-allocating, so we can keep item pointers
  // for later using.
  to_return.reserve(unused.size());

  for (ResourceId local_id : unused) {
    auto it = resources_.find(local_id);
    CHECK(it != resources_.end());
    ChildResource& resource = it->second;
    DCHECK(!resource.is_gpu_resource_type());

    auto sk_image_it = resource_sk_images_.find(local_id);
    if (sk_image_it != resource_sk_images_.end()) {
      resource_sk_images_.erase(sk_image_it);
    }

    ResourceId child_id = resource.transferable.id;
    DCHECK(child_info.child_to_parent_map.count(child_id));

    auto can_delete = CanDeleteNow(child_info, resource, style);
    if (can_delete == CanDeleteNowResult::kNo) {
      // Defer this resource deletion.
      resource.marked_for_deletion = true;
      continue;
    }

    const bool is_lost = can_delete == CanDeleteNowResult::kYesButLoseResource;

    to_return.emplace_back(child_id, resource.sync_token(),
                           std::move(resource.release_fence),
                           resource.imported_count, is_lost);

    child_info.child_to_parent_map.erase(child_id);
    resource.imported_count = 0;
    resources_.erase(it);
  }

  return to_return;
}

void DisplayResourceProviderSoftware::PopulateSkBitmapWithResource(
    SkBitmap* sk_bitmap,
    const ChildResource* resource,
    SkAlphaType alpha_type) {
  DCHECK(resource->transferable.format.IsBitmapFormatSupported());
  SkImageInfo info =
      SkImageInfo::MakeN32(resource->transferable.size.width(),
                           resource->transferable.size.height(), alpha_type);
  bool pixels_installed = sk_bitmap->installPixels(
      info, resource->shared_bitmap->pixels(), info.minRowBytes());
  DCHECK(pixels_installed);
}

DisplayResourceProviderSoftware::ScopedReadLockSkImage::ScopedReadLockSkImage(
    DisplayResourceProviderSoftware* resource_provider,
    ResourceId resource_id,
    SkAlphaType alpha_type)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const ChildResource* resource = resource_provider->LockForRead(resource_id);
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

  SkBitmap sk_bitmap;
  resource_provider->PopulateSkBitmapWithResource(&sk_bitmap, resource,
                                                  alpha_type);
  sk_bitmap.setImmutable();
  sk_image_ = SkImages::RasterFromBitmap(sk_bitmap);
  resource_provider_->resource_sk_images_[resource_id] = sk_image_;
}

DisplayResourceProviderSoftware::ScopedReadLockSkImage::
    ~ScopedReadLockSkImage() {
  resource_provider_->UnlockForRead(resource_id_);
}

}  // namespace viz
