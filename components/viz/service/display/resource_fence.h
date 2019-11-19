// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_FENCE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_FENCE_H_

#include "base/memory/ref_counted.h"

namespace viz {

// An abstract interface used to ensure reading from resources passed between
// client and service does not happen before writing is completed.
class ResourceFence : public base::RefCountedThreadSafe<ResourceFence> {
 public:
  virtual void Set() = 0;
  virtual bool HasPassed() = 0;

 protected:
  friend class base::RefCountedThreadSafe<ResourceFence>;
  ResourceFence() = default;
  virtual ~ResourceFence() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceFence);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RESOURCE_FENCE_H_
