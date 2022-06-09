// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_FENCE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_FENCE_H_

#include "base/memory/ref_counted.h"

#include "ui/gfx/gpu_fence_handle.h"

namespace viz {

// An abstract interface used to ensure reading from resources passed between
// client and service does not happen before writing is completed.
class ResourceFence : public base::RefCountedThreadSafe<ResourceFence> {
 public:
  ResourceFence(const ResourceFence&) = delete;
  ResourceFence& operator=(const ResourceFence&) = delete;

  // Notifies the fence is needed.
  virtual void Set() = 0;
  // Tells if the fence is ready.
  virtual bool HasPassed() = 0;
  // A release fence which availability depends on the type of resource fence
  // (managed by DisplayResourceProvider and
  // TransferableResource::synchronization_type). The client must ensure that
  // HasPassed is true before trying to access the release fence handle.
  // Otherwise, it's not guaranteed that the fence handle is valid.
  virtual gfx::GpuFenceHandle GetGpuFenceHandle() = 0;

 protected:
  friend class base::RefCountedThreadSafe<ResourceFence>;
  ResourceFence() = default;
  virtual ~ResourceFence() = default;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_FENCE_H_
