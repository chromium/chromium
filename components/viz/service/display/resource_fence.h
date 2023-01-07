// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_FENCE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_FENCE_H_

#include "base/memory/ref_counted.h"

#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/gpu_fence_handle.h"

namespace viz {

class DisplayResourceProvider;

// An abstract interface used to ensure reading from resources passed between
// client and service does not happen before writing is completed.
// Instances of this class are accessed from the display compositor thread.
class VIZ_SERVICE_EXPORT ResourceFence
    : public base::RefCounted<ResourceFence> {
 public:
  ResourceFence(const ResourceFence&) = delete;
  ResourceFence& operator=(const ResourceFence&) = delete;

  // Tells if the fence is ready.
  virtual bool HasPassed() = 0;
  // A release fence which availability depends on the type of resource fence
  // (managed by DisplayResourceProvider and
  // TransferableResource::synchronization_type). The client must ensure that
  // HasPassed is true before trying to access the release fence handle.
  // Otherwise, it's not guaranteed that the fence handle is valid.
  virtual gfx::GpuFenceHandle GetGpuFenceHandle() = 0;

  // Notifies the fence is needed.
  void set() { set_ = true; }
  bool was_set() const { return set_; }

  // Tracks a resource that will be released when this ResourceFence passes.
  void TrackDeferredResource(ResourceId id);

 protected:
  friend class base::RefCounted<ResourceFence>;

  explicit ResourceFence(DisplayResourceProvider* resource_provider);
  virtual ~ResourceFence();

  // Conveys to |resource_provider_| that this resource fence has passed.
  void FencePassed();

 private:
  bool set_ = false;
  base::flat_set<ResourceId> deferred_resources_;
  base::WeakPtr<DisplayResourceProvider> resource_provider_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_FENCE_H_
