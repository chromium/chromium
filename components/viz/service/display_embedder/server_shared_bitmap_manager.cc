// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"

#include <stdint.h>

#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

class BitmapData : public base::RefCounted<BitmapData> {
 public:
  explicit BitmapData(base::ReadOnlySharedMemoryMapping mapping)
      : mapping_(std::move(mapping)) {}

  const void* memory() const { return mapping_.memory(); }
  size_t size() const { return mapping_.size(); }
  const base::UnguessableToken& mapped_id() const { return mapping_.guid(); }

 private:
  friend class base::RefCounted<BitmapData>;
  ~BitmapData() {}

  base::ReadOnlySharedMemoryMapping mapping_;
  DISALLOW_COPY_AND_ASSIGN(BitmapData);
};

namespace {

// Holds a reference on the BitmapData so that the WritableSharedMemoryMapping
// can outlive the SharedBitmapId registration as long as this SharedBitmap
// object is held alive.
class ServerSharedBitmap : public SharedBitmap {
 public:
  // NOTE: bitmap_data->memory() is read-only but SharedBitmap expects a
  // uint8_t* pointer, even though all instances returned by a
  // SharedBitmapManager will be used read-only.
  explicit ServerSharedBitmap(scoped_refptr<BitmapData> bitmap_data)
      : SharedBitmap(
            static_cast<uint8_t*>(const_cast<void*>(bitmap_data->memory()))),
        bitmap_data_(std::move(bitmap_data)) {}

  ~ServerSharedBitmap() override {
  }

 private:
  scoped_refptr<BitmapData> bitmap_data_;
};

}  // namespace

ServerSharedBitmapManager::ServerSharedBitmapManager() = default;

ServerSharedBitmapManager::~ServerSharedBitmapManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(handle_map_.empty());
}

std::unique_ptr<SharedBitmap> ServerSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size& size,
    ResourceFormat format,
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = handle_map_.find(id);
  if (it == handle_map_.end()) {
    return nullptr;
  }

  BitmapData* data = it->second.get();

  size_t bitmap_size;
  if (!ResourceSizes::MaybeSizeInBytes(size, format, &bitmap_size) ||
      bitmap_size > data->size()) {
    return nullptr;
  }

  if (!data->memory()) {
    return nullptr;
  }

  return std::make_unique<ServerSharedBitmap>(data);
}

base::UnguessableToken
ServerSharedBitmapManager::GetSharedBitmapTracingGUIDFromId(
    const SharedBitmapId& id) {
  auto it = handle_map_.find(id);
  if (it == handle_map_.end())
    return {};
  BitmapData* data = it->second.get();
  return data->mapped_id();
}

bool ServerSharedBitmapManager::ChildAllocatedSharedBitmap(
    base::ReadOnlySharedMemoryMapping mapping,
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Duplicate ids are not allowed.
  if (base::Contains(handle_map_, id))
    return false;

  // This function handles public API requests, so verify we unwrapped a shared
  // memory handle before trying to use the handle.
  if (!mapping.IsValid())
    return false;

  handle_map_[id] = base::MakeRefCounted<BitmapData>(std::move(mapping));

  // Note: |region| will be destroyed at scope exit, releasing the fd.
  return true;
}

void ServerSharedBitmapManager::ChildDeletedSharedBitmap(
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  handle_map_.erase(id);
}

bool ServerSharedBitmapManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& pair : handle_map_) {
    const SharedBitmapId& id = pair.first;
    BitmapData* data = pair.second.get();

    std::string dump_str = base::StringPrintf(
        "sharedbitmap/%s", base::HexEncode(id.name, sizeof(id.name)).c_str());
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_str);
    if (!dump)
      return false;

    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    data->size());

    // This GUID is the same returned by GetSharedBitmapTracingGUIDFromId() so
    // other components use a consistent GUID for a given SharedBitmapId.
    base::UnguessableToken shared_memory_guid = data->mapped_id();
    DCHECK(!shared_memory_guid.is_empty());
    pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid,
                                         0 /* importance*/);
  }

  return true;
}

}  // namespace viz
