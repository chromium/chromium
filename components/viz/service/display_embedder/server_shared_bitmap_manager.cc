// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
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
  BitmapData() = default;
  BitmapData(const BitmapData& other) = delete;
  BitmapData& operator=(const BitmapData& other) = delete;

  virtual const void* GetMemory() const = 0;
  virtual size_t GetSize() const = 0;
  virtual const base::UnguessableToken& GetGUID() const = 0;

 protected:
  friend class base::RefCounted<BitmapData>;
  virtual ~BitmapData() = default;
};

// Holds a bitmap stored in local memory.
class LocalBitmapData : public BitmapData {
 public:
  explicit LocalBitmapData(SkBitmap bitmap)
      : bitmap_(std::move(bitmap)), guid_(base::UnguessableToken::Create()) {}

  const void* GetMemory() const override { return bitmap_.getPixels(); }
  size_t GetSize() const override { return bitmap_.computeByteSize(); }
  const base::UnguessableToken& GetGUID() const override { return guid_; }

 private:
  ~LocalBitmapData() override = default;

  SkBitmap bitmap_;
  // GUID to identify this bitmap in memory dumps.
  base::UnguessableToken guid_;
};

// Holds a bitmap stored in shared memory.
class SharedMemoryBitmapData : public BitmapData {
 public:
  explicit SharedMemoryBitmapData(base::ReadOnlySharedMemoryMapping mapping)
      : mapping_(std::move(mapping)) {}

  const void* GetMemory() const override { return mapping_.memory(); }
  size_t GetSize() const override { return mapping_.size(); }
  const base::UnguessableToken& GetGUID() const override {
    return mapping_.guid();
  }

 private:
  ~SharedMemoryBitmapData() override = default;

  base::ReadOnlySharedMemoryMapping mapping_;
};

namespace {

// Holds a reference on the BitmapData so that the WritableSharedMemoryMapping
// can outlive the SharedBitmapId registration as long as this SharedBitmap
// object is held alive.
class ServerSharedBitmap : public SharedBitmap {
 public:
  // NOTE: bitmap_data->GetMemory() is read-only but SharedBitmap expects a
  // uint8_t* pointer, even though all instances returned by a
  // SharedBitmapManager will be used read-only.
  explicit ServerSharedBitmap(scoped_refptr<BitmapData> bitmap_data)
      : SharedBitmap(
            static_cast<uint8_t*>(const_cast<void*>(bitmap_data->GetMemory()))),
        bitmap_data_(std::move(bitmap_data)) {}

  ~ServerSharedBitmap() override {
    // Drop unowned reference before destroying `bitmap_data_`.
    pixels_ = nullptr;
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
    SharedImageFormat format,
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = handle_map_.find(id);
  if (it == handle_map_.end()) {
    return nullptr;
  }

  BitmapData* data = it->second.get();

  size_t bitmap_size;
  if (!ResourceSizes::MaybeSizeInBytes(size, format, &bitmap_size) ||
      bitmap_size > data->GetSize()) {
    return nullptr;
  }

  if (!data->GetMemory()) {
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
  return data->GetGUID();
}

bool ServerSharedBitmapManager::LocalAllocatedSharedBitmap(
    SkBitmap bitmap,
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bitmap.drawsNothing());

  // Duplicate ids are not allowed.
  if (base::Contains(handle_map_, id))
    return false;

  handle_map_[id] = base::MakeRefCounted<LocalBitmapData>(std::move(bitmap));

  return true;
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

  handle_map_[id] =
      base::MakeRefCounted<SharedMemoryBitmapData>(std::move(mapping));

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
        "sharedbitmap/%s",
        base::HexEncode(base::as_byte_span(id.name)).c_str());
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_str);
    if (!dump)
      return false;

    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    data->GetSize());

    // This GUID is the same returned by GetSharedBitmapTracingGUIDFromId() so
    // other components use a consistent GUID for a given SharedBitmapId.
    base::UnguessableToken bitmap_guid = data->GetGUID();
    DCHECK(!bitmap_guid.is_empty());
    pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), bitmap_guid,
                                         /*importance=*/0);
  }

  return true;
}

}  // namespace viz
