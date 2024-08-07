// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_SOFTWARE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_SOFTWARE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gpu {
class SharedImageManager;
class SyncPointManager;
class Scheduler;
}

namespace viz {

class SharedBitmapManager;

// DisplayResourceProvider implementation used with SoftwareRenderer.
class VIZ_SERVICE_EXPORT DisplayResourceProviderSoftware
    : public DisplayResourceProvider {
 public:
  explicit DisplayResourceProviderSoftware(
      SharedBitmapManager* shared_bitmap_manager,
      gpu::SharedImageManager* shared_image_manager,
      gpu::SyncPointManager* sync_point_manager,
      gpu::Scheduler* scheduler);
  ~DisplayResourceProviderSoftware() override;

  class VIZ_SERVICE_EXPORT ScopedReadLockSkImage {
   public:
    ScopedReadLockSkImage(DisplayResourceProviderSoftware* resource_provider,
                          ResourceId resource_id,
                          SkAlphaType alpha_type);
    ~ScopedReadLockSkImage();

    ScopedReadLockSkImage(const ScopedReadLockSkImage&) = delete;
    ScopedReadLockSkImage& operator=(const ScopedReadLockSkImage& other) =
        delete;

    const SkImage* sk_image() const { return sk_image_.get(); }
    sk_sp<SkImage> TakeSkImage() { return std::move(sk_image_); }

    bool valid() const { return !!sk_image_; }

   private:
    const raw_ptr<DisplayResourceProviderSoftware> resource_provider_;
    const ResourceId resource_id_;
    sk_sp<SkImage> sk_image_;
  };

  // Waits on the SyncToken and returns MemoryImageRepresentation of the
  // SharedImage pointed by mailbox.
  std::unique_ptr<gpu::MemoryImageRepresentation> GetSharedImageRepresentation(
      const gpu::Mailbox& mailbox,
      const gpu::SyncToken& sync_token);

 private:
  // These functions are used by ScopedReadLockSkImage to lock and unlock
  // resources.
  const ChildResource* LockForRead(ResourceId id);
  void UnlockForRead(ResourceId id, const SkImage* sk_image);

  // DisplayResourceProvider overrides:
  std::vector<ReturnedResource> DeleteAndReturnUnusedResourcesToChildImpl(
      Child& child_info,
      DeleteStyle style,
      const std::vector<ResourceId>& unused) override;

  void PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                    const ChildResource* resource,
                                    SkAlphaType alpha_type);
  void WaitSyncToken(gpu::SyncToken sync_token);

  const raw_ptr<SharedBitmapManager> shared_bitmap_manager_;
  const raw_ptr<gpu::SharedImageManager> shared_image_manager_;
  const raw_ptr<gpu::SyncPointManager> sync_point_manager_;
  const raw_ptr<gpu::Scheduler> gpu_scheduler_;
  scoped_refptr<gpu::SyncPointOrderData> sync_point_order_data_;

  base::flat_map<ResourceId, sk_sp<SkImage>> resource_sk_images_;

  std::unique_ptr<gpu::MemoryTypeTracker> memory_tracker_;

  struct SharedImageAccess {
    SharedImageAccess();
    ~SharedImageAccess();
    SharedImageAccess(SharedImageAccess&& other);
    SharedImageAccess& operator=(SharedImageAccess&& other);

    SharedImageAccess(const SharedImageAccess&) = delete;
    SharedImageAccess& operator=(const SharedImageAccess&) = delete;

    std::unique_ptr<gpu::MemoryImageRepresentation> representation;
    std::unique_ptr<gpu::MemoryImageRepresentation::ScopedReadAccess>
        read_access;
  };

  base::flat_map<ResourceId, std::unique_ptr<SharedImageAccess>>
      resource_shared_images_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_SOFTWARE_H_
