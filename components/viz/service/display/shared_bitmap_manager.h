// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SHARED_BITMAP_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SHARED_BITMAP_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/shared_memory_mapping.h"
#include "components/viz/common/resources/shared_bitmap.h"

namespace gfx {
class Size;
}

namespace viz {

class SharedBitmapManager {
 public:
  SharedBitmapManager() {}
  virtual ~SharedBitmapManager() {}

  // Used in the display compositor to find the bitmap associated with an id.
  virtual std::unique_ptr<SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size& size,
      ResourceFormat format,
      const SharedBitmapId& id) = 0;
  virtual base::UnguessableToken GetSharedBitmapTracingGUIDFromId(
      const SharedBitmapId& id) = 0;
  // Used in the display compositor to associate an id to a shm mapping.
  virtual bool ChildAllocatedSharedBitmap(
      base::ReadOnlySharedMemoryMapping mapping,
      const SharedBitmapId& id) = 0;
  // Used in the display compositor to break an association of an id to a shm
  // handle.
  virtual void ChildDeletedSharedBitmap(const SharedBitmapId& id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedBitmapManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SHARED_BITMAP_MANAGER_H_
