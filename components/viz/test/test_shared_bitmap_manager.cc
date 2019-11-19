// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_shared_bitmap_manager.h"

#include <stdint.h>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace viz {

TestSharedBitmapManager::TestSharedBitmapManager() = default;

TestSharedBitmapManager::~TestSharedBitmapManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<SharedBitmap> TestSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size&,
    ResourceFormat,
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto it = mapping_map_.find(id);
  if (it == mapping_map_.end())
    return nullptr;
  // NOTE: pixels needs to be writable for legacy reasons, but SharedBitmap
  // instances returned by a SharedBitmapManager are always read-only.
  auto* pixels = static_cast<uint8_t*>(const_cast<void*>(it->second.memory()));
  return std::make_unique<SharedBitmap>(pixels);
}

base::UnguessableToken
TestSharedBitmapManager::GetSharedBitmapTracingGUIDFromId(
    const SharedBitmapId& id) {
  const auto it = mapping_map_.find(id);
  if (it == mapping_map_.end())
    return {};
  return it->second.guid();
}

bool TestSharedBitmapManager::ChildAllocatedSharedBitmap(
    base::ReadOnlySharedMemoryMapping mapping,
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TestSharedBitmapManager is both the client and service side. So the
  // notification here should be about a bitmap that was previously allocated
  // with AllocateSharedBitmap().
  if (mapping_map_.find(id) == mapping_map_.end()) {
    mapping_map_.emplace(id, std::move(mapping));
  }

  // The same bitmap id should not be notified more than once.
  DCHECK_EQ(notified_set_.count(id), 0u);
  notified_set_.insert(id);
  return true;
}

void TestSharedBitmapManager::ChildDeletedSharedBitmap(
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  notified_set_.erase(id);
  mapping_map_.erase(id);
}

}  // namespace viz
