// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_device_backing.h"

#include <algorithm>
#include <vector>

#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/viz/common/resources/resource_sizes.h"

namespace viz {
namespace {

// If a window is larger than this in bytes, don't even try to create a backing
// bitmap for it.
constexpr size_t kMaxBitmapSizeBytes = 4 * (16384 * 8192);

// Finds the size in bytes to hold |viewport_size| pixels. If |viewport_size| is
// a valid size this will return true and |out_bytes| will contain the size in
// bytes. If |viewport_size| is not a valid size then this will return false.
bool GetViewportSizeInBytes(const gfx::Size& viewport_size, size_t* out_bytes) {
  size_t bytes;
  if (!ResourceSizes::MaybeSizeInBytes(viewport_size,
                                       SinglePlaneFormat::kRGBA_8888, &bytes)) {
    return false;
  }
  if (bytes > kMaxBitmapSizeBytes)
    return false;
  *out_bytes = bytes;
  return true;
}

}  // namespace

OutputDeviceBacking::OutputDeviceBacking() = default;

OutputDeviceBacking::~OutputDeviceBacking() {
  DCHECK(clients_.empty());
}

void OutputDeviceBacking::ClientResized() {
  // If the max viewport size doesn't change then nothing here changes.
  if (GetMaxViewportBytes() == created_shm_bytes_)
    return;

  // Otherwise we need to allocate a new shared memory region and clients
  // should re-request it.
  for (OutputDeviceBacking::Client* client : clients_) {
    client->ReleaseCanvas();
  }

  region_ = {};
  created_shm_bytes_ = 0;
}

void OutputDeviceBacking::RegisterClient(Client* client) {
  clients_.push_back(client);
}

void OutputDeviceBacking::UnregisterClient(Client* client) {
  DCHECK(base::Contains(clients_, client));
  std::erase(clients_, client);
  ClientResized();
}

base::UnsafeSharedMemoryRegion* OutputDeviceBacking::GetSharedMemoryRegion(
    const gfx::Size& viewport_size) {
  // If |viewport_size| is empty or too big don't try to allocate SharedMemory.
  size_t viewport_bytes;
  if (!GetViewportSizeInBytes(viewport_size, &viewport_bytes))
    return nullptr;

  // Allocate a new SharedMemory segment that can fit the largest viewport.
  if (!region_.IsValid()) {
    size_t max_viewport_bytes = GetMaxViewportBytes();
    DCHECK_LE(viewport_bytes, max_viewport_bytes);

    base::debug::Alias(&max_viewport_bytes);
    region_ = base::UnsafeSharedMemoryRegion::Create(max_viewport_bytes);
    if (!region_.IsValid()) {
      LOG(ERROR) << "Shared memory region create failed on "
                 << max_viewport_bytes << " bytes";
      return nullptr;
    }
    created_shm_bytes_ = max_viewport_bytes;
  } else {
    // Clients must call Resize() for new |viewport_size|.
    DCHECK_LE(viewport_bytes, created_shm_bytes_);
  }

  return &region_;
}

size_t OutputDeviceBacking::GetMaxViewportBytes() {
  // Minimum byte size is 1 because creating a 0-byte-long SharedMemory fails.
  size_t max_bytes = 1;
  for (OutputDeviceBacking::Client* client : clients_) {
    size_t current_bytes;
    if (!GetViewportSizeInBytes(client->GetViewportPixelSize(), &current_bytes))
      continue;
    max_bytes = std::max(max_bytes, current_bytes);
  }
  return max_bytes;
}

}  // namespace viz
