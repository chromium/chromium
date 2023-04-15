// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_SHARED_BITMAP_MANAGER_H_
#define COMPONENTS_VIZ_TEST_TEST_SHARED_BITMAP_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/memory/shared_memory_mapping.h"
#include "base/sequence_checker.h"
#include "components/viz/service/display/shared_bitmap_manager.h"

namespace viz {

class TestSharedBitmapManager : public SharedBitmapManager {
 public:
  TestSharedBitmapManager();
  ~TestSharedBitmapManager() override;

  // SharedBitmapManager implementation.
  std::unique_ptr<SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size& size,
      SharedImageFormat format,
      const SharedBitmapId& id) override;
  base::UnguessableToken GetSharedBitmapTracingGUIDFromId(
      const SharedBitmapId& id) override;
  bool ChildAllocatedSharedBitmap(base::ReadOnlySharedMemoryMapping mapping,
                                  const SharedBitmapId& id) override;
  bool LocalAllocatedSharedBitmap(SkBitmap bitmap,
                                  const SharedBitmapId& id) override;
  void ChildDeletedSharedBitmap(const SharedBitmapId& id) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::map<SharedBitmapId, base::ReadOnlySharedMemoryMapping> mapping_map_;
  std::set<SharedBitmapId> notified_set_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_SHARED_BITMAP_MANAGER_H_
