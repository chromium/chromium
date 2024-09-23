// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_skia.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "components/viz/service/display/resource_fence.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace viz {

class ScopedAllowGpuAccessForDisplayResourceProvider {
 public:
  ~ScopedAllowGpuAccessForDisplayResourceProvider() = default;

  explicit ScopedAllowGpuAccessForDisplayResourceProvider(
      DisplayResourceProvider* provider) {
    DCHECK(provider->can_access_gpu_thread_);
  }

 private:
  gpu::ScopedAllowScheduleGpuTask allow_gpu_;
};

DisplayResourceProviderSkia::DisplayResourceProviderSkia()
    : DisplayResourceProvider(DisplayResourceProvider::kGpu) {}

DisplayResourceProviderSkia::~DisplayResourceProviderSkia() {
  Destroy();
}

std::vector<ReturnedResource>
DisplayResourceProviderSkia::DeleteAndReturnUnusedResourcesToChildImpl(
    Child& child_info,
    DeleteStyle style,
    const std::vector<ResourceId>& unused) {
  std::vector<ReturnedResource> to_return;
  std::vector<std::unique_ptr<ExternalUseClient::ImageContext>>
      image_contexts_to_return;
  std::vector<ReturnedResource*> external_used_resources;

  // Reserve enough space to avoid re-allocating, so we can keep item pointers
  // for later using.
  to_return.reserve(unused.size());
  image_contexts_to_return.reserve(unused.size());
  external_used_resources.reserve(unused.size());

  DCHECK(external_use_client_);
  std::vector<ResourceId>* batch_return = nullptr;

  for (ResourceId local_id : unused) {
    auto it = resources_.find(local_id);
    CHECK(it != resources_.end());
    ChildResource& resource = it->second;

    if (!can_access_gpu_thread_) {
      // We should always has access to gpu thread during shutdown.
      DCHECK(style != DeleteStyle::FOR_SHUTDOWN);

      // If we don't have access to gpu thread, we can't free it right now.
      if (resource.image_context) {
        if (!batch_return) {
          batch_return = &batched_returning_resources_[child_info.id];
        }
        batch_return->push_back(local_id);
        continue;
      }
    }

    ResourceId child_id = resource.transferable.id;
    DCHECK(child_info.child_to_parent_map.count(child_id));
    DCHECK(resource.is_gpu_resource_type());

    auto can_delete = CanDeleteNow(child_info, resource, style);
    if (can_delete == CanDeleteNowResult::kNo) {
      // Defer this resource deletion.
      resource.marked_for_deletion = true;
      continue;
    }

    if (resource.transferable.synchronization_type ==
        TransferableResource::SynchronizationType::kReleaseFence) {
      // The resource might have never been used.
      if (resource.resource_fence)
        resource.release_fence = resource.resource_fence->GetGpuFenceHandle();
    }

    const bool is_lost = can_delete == CanDeleteNowResult::kYesButLoseResource;

    to_return.emplace_back(child_id, resource.sync_token(),
                           std::move(resource.release_fence),
                           resource.imported_count, is_lost);
    auto& returned = to_return.back();

    if (resource.image_context) {
      image_contexts_to_return.emplace_back(std::move(resource.image_context));
      external_used_resources.push_back(&returned);
    }

    child_info.child_to_parent_map.erase(child_id);
    resource.imported_count = 0;
    resources_.erase(it);
  }

  if (!image_contexts_to_return.empty()) {
    ScopedAllowGpuAccessForDisplayResourceProvider allow_gpu(this);
    gpu::SyncToken sync_token = external_use_client_->ReleaseImageContexts(
        std::move(image_contexts_to_return));
    for (auto* resource : external_used_resources) {
      resource->sync_token = sync_token;
    }
  }

  return to_return;
}

DisplayResourceProviderSkia::LockSetForExternalUse::LockSetForExternalUse(
    DisplayResourceProviderSkia* resource_provider,
    ExternalUseClient* client)
    : resource_provider_(resource_provider) {
  DCHECK(!resource_provider_->external_use_client_);
  DCHECK(client);
  resource_provider_->external_use_client_ = client;
}

DisplayResourceProviderSkia::LockSetForExternalUse::~LockSetForExternalUse() {
  DCHECK(resources_.empty());
}

ExternalUseClient::ImageContext*
DisplayResourceProviderSkia::LockSetForExternalUse::LockResource(
    ResourceId id,
    bool maybe_concurrent_reads,
    bool raw_draw_is_possible) {
  auto it = resource_provider_->resources_.find(id);
  CHECK(it != resource_provider_->resources_.end(), base::NotFatalUntil::M130);

  ChildResource& resource = it->second;
  DCHECK(resource.is_gpu_resource_type());

  if (!resource.locked_for_external_use) {
    DCHECK(!base::Contains(resources_, std::make_pair(id, &resource)));
    resources_.emplace_back(id, &resource);

    if (!resource.image_context) {
      // SkColorSpace covers only RGB portion of the gfx::ColorSpace, YUV
      // portion is handled via SkYuvColorSpace at places where we create YUV
      // images.
      sk_sp<SkColorSpace> image_color_space =
          resource.transferable.color_space.GetAsFullRangeRGB()
              .ToSkColorSpace();

      resource.image_context =
          resource_provider_->external_use_client_->CreateImageContext(
              gpu::MailboxHolder(resource.transferable.mailbox(),
                                 resource.transferable.sync_token(),
                                 resource.transferable.texture_target()),
              resource.transferable.size, resource.transferable.format,
              maybe_concurrent_reads, resource.transferable.ycbcr_info,
              std::move(image_color_space), raw_draw_is_possible);
    }
    resource.locked_for_external_use = true;

    switch (resource.transferable.synchronization_type) {
      case TransferableResource::SynchronizationType::kGpuCommandsCompleted:
        resource.resource_fence =
            resource_provider_->current_gpu_commands_completed_fence_;
        break;
      case TransferableResource::SynchronizationType::kReleaseFence:
        resource.resource_fence = resource_provider_->current_release_fence_;
        break;
      default:
        break;
    }

    if (resource.resource_fence) {
      resource.resource_fence->set();
      resource.resource_fence->TrackDeferredResource(id);
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

DisplayResourceProviderSkia::ScopedExclusiveReadLockSharedImage::
    ScopedExclusiveReadLockSharedImage(
        DisplayResourceProviderSkia* resource_provider,
        ResourceId resource_id)
    : ScopedReadLockSharedImage(resource_provider, resource_id) {
  ChildResource& resource = *this->resource();
  if (resource.image_context) {
    DCHECK(!resource.locked_for_external_use)
        << "Resource already locked, can't get exclusive lock!";
    DCHECK(resource_provider->can_access_gpu_thread_)
        << "Can't release |image_context| without access to gpu thread";

    std::vector<std::unique_ptr<ExternalUseClient::ImageContext>>
        image_contexts;
    image_contexts.push_back(std::move(resource.image_context));

    gpu::SyncToken sync_token =
        resource_provider->external_use_client_->ReleaseImageContexts(
            std::move(image_contexts));
    resource.UpdateSyncToken(sync_token);
  }
}
DisplayResourceProviderSkia::ScopedExclusiveReadLockSharedImage::
    ~ScopedExclusiveReadLockSharedImage() = default;

DisplayResourceProviderSkia::ScopedExclusiveReadLockSharedImage::
    ScopedExclusiveReadLockSharedImage(
        ScopedExclusiveReadLockSharedImage&& other) = default;

}  // namespace viz
