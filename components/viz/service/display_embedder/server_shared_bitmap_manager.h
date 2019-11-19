// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SERVER_SHARED_BITMAP_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SERVER_SHARED_BITMAP_MANAGER_H_

#include <memory>
#include <unordered_map>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/unguessable_token.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
class BitmapData;

// A SharedBitmapManager implementation that lives in-process with the
// display compositor. It manages mappings from SharedBitmapId to
// SharedMemory segments. While the returned SharedBitmap is kept alive
// for a given SharedBitmapId, the backing pixels are guaranteed to remain
// valid.
class VIZ_SERVICE_EXPORT ServerSharedBitmapManager
    : public SharedBitmapManager,
      public base::trace_event::MemoryDumpProvider {
 public:
  ServerSharedBitmapManager();
  ~ServerSharedBitmapManager() override;

  // SharedBitmapManager implementation.
  std::unique_ptr<SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size& size,
      ResourceFormat format,
      const SharedBitmapId& id) override;
  base::UnguessableToken GetSharedBitmapTracingGUIDFromId(
      const SharedBitmapId& id) override;
  bool ChildAllocatedSharedBitmap(base::ReadOnlySharedMemoryMapping mapping,
                                  const SharedBitmapId& id) override;
  void ChildDeletedSharedBitmap(const SharedBitmapId& id) override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::unordered_map<SharedBitmapId,
                     scoped_refptr<BitmapData>,
                     SharedBitmapIdHash>
      handle_map_;

  DISALLOW_COPY_AND_ASSIGN(ServerSharedBitmapManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SERVER_SHARED_BITMAP_MANAGER_H_
