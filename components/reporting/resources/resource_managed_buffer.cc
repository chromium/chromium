// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/resources/resource_managed_buffer.h"

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/reporting/util/status.h"

namespace reporting {

ResourceManagedBuffer::ResourceManagedBuffer(
    scoped_refptr<ResourceManager> memory_resource)
    : memory_resource_(memory_resource) {}

ResourceManagedBuffer::~ResourceManagedBuffer() {
  Clear();
}

Status ResourceManagedBuffer::Allocate(size_t size) {
  // Lose whatever was allocated before (if any).
  Clear();
  // Register with resource management.
  if (!memory_resource_->Reserve(size)) {
    return Status(error::RESOURCE_EXHAUSTED,
                  "Not enough memory for the buffer");
  }
  // Commit memory allocation.
  buffer_ = base::HeapArray<uint8_t>::WithSize(size);
  return Status::StatusOK();
}

void ResourceManagedBuffer::Clear() {
  if (!empty()) {
    memory_resource_->Discard(size());
    buffer_ = {};
  }
}

}  // namespace reporting
