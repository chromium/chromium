// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_software.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "third_party/skia/include/core/SkImage.h"

namespace viz {

DisplayResourceProviderSoftware::DisplayResourceProviderSoftware(
    SharedBitmapManager* shared_bitmap_manager,
    gpu::SharedImageManager* shared_image_manager,
    gpu::SyncPointManager* sync_point_manager,
    gpu::Scheduler* scheduler)
    : DisplayResourceProvider(DisplayResourceProvider::kSoftware),
      shared_bitmap_manager_(shared_bitmap_manager),
      shared_image_manager_(shared_image_manager),
      sync_point_manager_(sync_point_manager),
      gpu_scheduler_(scheduler),
      sync_point_order_data_(
          sync_point_manager_ ? sync_point_manager_->CreateSyncPointOrderData()
                              : nullptr) {
  memory_tracker_ = std::make_unique<gpu::MemoryTypeTracker>(nullptr);
}

DisplayResourceProviderSoftware::~DisplayResourceProviderSoftware() {
  Destroy();
  if (sync_point_order_data_) {
    sync_point_order_data_->Destroy();
  }
}

std::unique_ptr<gpu::MemoryImageRepresentation>
DisplayResourceProviderSoftware::GetSharedImageRepresentation(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token) {
  WaitSyncToken(sync_token);
  return shared_image_manager_->ProduceMemory(mailbox, memory_tracker_.get());
}

const DisplayResourceProvider::ChildResource*
DisplayResourceProviderSoftware::LockForRead(ResourceId id) {
  ChildResource* resource = GetResource(id);
  DCHECK(!resource->is_gpu_resource_type());

  // Determine whether this resource is using a software SharedImage or a legacy
  // shared bitmap.
  if (resource->transferable.IsSoftwareSharedImage()) {
    DCHECK(shared_image_manager_ && sync_point_manager_);
    auto it = resource_shared_images_.find(id);
    if (it == resource_shared_images_.end()) {
      const gpu::Mailbox& mailbox = resource->transferable.mailbox();
      auto access = std::make_unique<SharedImageAccess>();
      access->representation = GetSharedImageRepresentation(
          mailbox, resource->transferable.sync_token());
      if (!access->representation) {
        return nullptr;
      }

      access->read_access = access->representation->BeginScopedReadAccess();
      resource_shared_images_.emplace(id, std::move(access));
    }
  } else {
    if (!resource->shared_bitmap) {
      const SharedBitmapId& shared_bitmap_id =
          resource->transferable.shared_bitmap_id();
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
  }

  resource->lock_for_read_count++;
  return resource;
}

void DisplayResourceProviderSoftware::UnlockForRead(ResourceId id,
                                                    const SkImage* sk_image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ChildResource* resource = GetResource(id);
  DCHECK(!resource->is_gpu_resource_type());
  if (sk_image) {
    DCHECK_GE(resource->lock_for_read_count, 0);
    resource->lock_for_read_count--;
  }
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

    auto si_image_it = resource_shared_images_.find(local_id);
    if (si_image_it != resource_shared_images_.end()) {
      resource_shared_images_.erase(si_image_it);
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
  if (!resource) {
    return;
  }
  DCHECK(!resource->is_gpu_resource_type());

  // Use cached SkImage if possible.
  auto it = resource_provider_->resource_sk_images_.find(resource_id);
  if (it != resource_provider_->resource_sk_images_.end()) {
    sk_image_ = it->second;
    return;
  }

  auto si_image_it =
      resource_provider->resource_shared_images_.find(resource_id);

  if (resource->shared_bitmap) {
    SkBitmap sk_bitmap;
    resource_provider->PopulateSkBitmapWithResource(&sk_bitmap, resource,
                                                    alpha_type);
    sk_bitmap.setImmutable();
    sk_image_ = SkImages::RasterFromBitmap(sk_bitmap);
    resource_provider_->resource_sk_images_[resource_id] = sk_image_;
  } else if (si_image_it != resource_provider->resource_shared_images_.end()) {
    sk_image_ = SkImages::RasterFromPixmap(
        si_image_it->second->read_access->pixmap(), nullptr, nullptr);
    resource_provider_->resource_sk_images_[resource_id] = sk_image_;
  } else {
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
}

void DisplayResourceProviderSoftware::WaitSyncToken(gpu::SyncToken sync_token) {
  uint32_t order_num = sync_point_order_data_->GenerateUnprocessedOrderNumber();
  base::WaitableEvent completion;
  if (sync_point_manager_->Wait(
          sync_token, sync_point_order_data_->sequence_id(), order_num,
          base::BindOnce(&base::WaitableEvent::Signal,
                         base::Unretained(&completion)))) {
    gpu::SequenceId release_sequence_id =
        sync_point_manager_->GetSyncTokenReleaseSequenceId(sync_token);
    gpu::Scheduler::ScopedSetSequencePriority waiting(
        gpu_scheduler_, release_sequence_id, gpu::SchedulingPriority::kHigh);

    completion.Wait();
  }

  // We don't have any tasks to run here, but we need to mark order number as
  // complete.
  sync_point_order_data_->BeginProcessingOrderNumber(order_num);
  sync_point_order_data_->FinishProcessingOrderNumber(order_num);
}

DisplayResourceProviderSoftware::ScopedReadLockSkImage::
    ~ScopedReadLockSkImage() {
  resource_provider_->UnlockForRead(resource_id_, sk_image_.get());
}

DisplayResourceProviderSoftware::SharedImageAccess::SharedImageAccess() =
    default;
DisplayResourceProviderSoftware::SharedImageAccess::~SharedImageAccess() =
    default;
DisplayResourceProviderSoftware::SharedImageAccess::SharedImageAccess(
    SharedImageAccess&& other) = default;
DisplayResourceProviderSoftware::SharedImageAccess&
DisplayResourceProviderSoftware::SharedImageAccess::operator=(
    SharedImageAccess&& other) = default;

}  // namespace viz
