// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"

#include <utility>

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

namespace viz {

ScopedSurfaceIdAllocator::ScopedSurfaceIdAllocator(
    base::OnceCallback<void()> allocation_task)
    : allocation_task_(std::move(allocation_task)) {}

ScopedSurfaceIdAllocator::ScopedSurfaceIdAllocator(
    ParentLocalSurfaceIdAllocator* allocator,
    base::OnceCallback<void()> allocation_task)
    : allocator_(allocator), allocation_task_(std::move(allocation_task)) {
  // If you hit this DCHECK, it is because you are attempting to allow multiple
  // suppressions to be in flight at the same time.
  DCHECK(!allocator->is_allocation_suppressed_);
  allocator->is_allocation_suppressed_ = true;
}

ScopedSurfaceIdAllocator::ScopedSurfaceIdAllocator(
    ScopedSurfaceIdAllocator&& other)
    : allocator_(std::move(other.allocator_)),
      allocation_task_(std::move(other.allocation_task_)) {
  other.allocator_ = nullptr;
  DCHECK(other.allocation_task_.is_null());
}

ScopedSurfaceIdAllocator& ScopedSurfaceIdAllocator::operator=(
    ScopedSurfaceIdAllocator&& other) {
  ScopedSurfaceIdAllocator temp(std::move(other));
  swap(*this, temp);
  return *this;
}

ScopedSurfaceIdAllocator::~ScopedSurfaceIdAllocator() {
  if (allocator_) {
    DCHECK(allocator_->is_allocation_suppressed_);
    allocator_->is_allocation_suppressed_ = false;
  }

  if (allocation_task_)
    std::move(allocation_task_).Run();
}

void swap(ScopedSurfaceIdAllocator& first, ScopedSurfaceIdAllocator& second) {
  using std::swap;  // to enable ADL

  swap(first.allocator_, second.allocator_);
  swap(first.allocation_task_, second.allocation_task_);
}

}  // namespace viz
