// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_SOFTWARE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_SOFTWARE_H_

#include <utility>

#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// DisplayResourceProvider implementation used with SoftwareRenderer.
class VIZ_SERVICE_EXPORT DisplayResourceProviderSoftware
    : public DisplayResourceProvider {
 public:
  explicit DisplayResourceProviderSoftware(
      SharedBitmapManager* shared_bitmap_manager);

  class VIZ_SERVICE_EXPORT ScopedReadLockSkImage {
   public:
    ScopedReadLockSkImage(DisplayResourceProviderSoftware* resource_provider,
                          ResourceId resource_id,
                          SkAlphaType alpha_type = kPremul_SkAlphaType,
                          GrSurfaceOrigin origin = kTopLeft_GrSurfaceOrigin);
    ~ScopedReadLockSkImage();

    ScopedReadLockSkImage(const ScopedReadLockSkImage&) = delete;
    ScopedReadLockSkImage& operator=(const ScopedReadLockSkImage& other) =
        delete;

    const SkImage* sk_image() const { return sk_image_.get(); }
    sk_sp<SkImage> TakeSkImage() { return std::move(sk_image_); }

    bool valid() const { return !!sk_image_; }

   private:
    DisplayResourceProviderSoftware* const resource_provider_;
    const ResourceId resource_id_;
    sk_sp<SkImage> sk_image_;
  };

 private:
  // These functions are used by ScopedReadLockSkImage to lock and unlock
  // resources.
  const ChildResource* LockForRead(ResourceId id);
  void UnlockForRead(ResourceId id);

  void PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                    const ChildResource* resource);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_SOFTWARE_H_
