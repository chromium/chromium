// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_skia.h"

#include <utility>

namespace viz {

DisplayResourceProviderSkia::DisplayResourceProviderSkia(
    SharedBitmapManager* shared_bitmap_manager)
    : DisplayResourceProvider(DisplayResourceProvider::kGpu,
                              /*compositor_context_provider=*/nullptr,
                              shared_bitmap_manager,
                              /*enable_shared_images=*/true) {}

DisplayResourceProviderSkia::LockSetForExternalUse::LockSetForExternalUse(
    DisplayResourceProviderSkia* resource_provider,
    ExternalUseClient* client)
    : resource_provider_(resource_provider) {
  DCHECK(!resource_provider_->external_use_client_);
  resource_provider_->external_use_client_ = client;
}

DisplayResourceProviderSkia::LockSetForExternalUse::~LockSetForExternalUse() {
  DCHECK(resources_.empty());
}

ExternalUseClient::ImageContext*
DisplayResourceProviderSkia::LockSetForExternalUse::LockResource(
    ResourceId id,
    bool maybe_concurrent_reads,
    bool is_video_plane,
    const gfx::ColorSpace& color_space) {
  auto it = resource_provider_->resources_.find(id);
  DCHECK(it != resource_provider_->resources_.end());

  ChildResource& resource = it->second;
  DCHECK(resource.is_gpu_resource_type());

  if (!resource.locked_for_external_use) {
    DCHECK(!base::Contains(resources_, std::make_pair(id, &resource)));
    resources_.emplace_back(id, &resource);

    if (!resource.image_context) {
      sk_sp<SkColorSpace> image_color_space;
      if (!is_video_plane) {
        // HDR video color conversion is handled externally in SkiaRenderer
        // using a special color filter and |color_space| is set to destination
        // color space so that Skia doesn't perform implicit color conversion.
        image_color_space =
            color_space.IsValid()
                ? color_space.ToSkColorSpace()
                : resource.transferable.color_space.ToSkColorSpace();
      }
      resource.image_context =
          resource_provider_->external_use_client_->CreateImageContext(
              resource.transferable.mailbox_holder, resource.transferable.size,
              resource.transferable.format, maybe_concurrent_reads,
              resource.transferable.ycbcr_info, std::move(image_color_space));
    }
    resource.locked_for_external_use = true;

    if (resource.transferable.read_lock_fences_enabled) {
      if (resource_provider_->current_read_lock_fence_.get())
        resource_provider_->current_read_lock_fence_->Set();
      resource.read_lock_fence = resource_provider_->current_read_lock_fence_;
    }
  }

  DCHECK(base::Contains(resources_, std::make_pair(id, &resource)));
  return resource.image_context.get();
}

void DisplayResourceProviderSkia::LockSetForExternalUse::UnlockResources(
    const gpu::SyncToken& sync_token) {
  DCHECK(sync_token.verified_flush());
  for (const auto& pair : resources_) {
    auto id = pair.first;
    auto* resource = pair.second;
    DCHECK(resource->locked_for_external_use);

    // TODO(penghuang): support software resource.
    DCHECK(resource->is_gpu_resource_type());

    // Update the resource sync token to |sync_token|. When the next frame is
    // being composited, the DeclareUsedResourcesFromChild() will be called with
    // resources belong to every child for the next frame. If the resource is
    // not used by the next frame, the resource will be returned to a child
    // which owns it with the |sync_token|. The child is responsible for issuing
    // a WaitSyncToken GL command with the |sync_token| before reusing it.
    resource->UpdateSyncToken(sync_token);
    resource->locked_for_external_use = false;

    resource_provider_->TryReleaseResource(id, resource);
  }
  resources_.clear();
}

}  // namespace viz
