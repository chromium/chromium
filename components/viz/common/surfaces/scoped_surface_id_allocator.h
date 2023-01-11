// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_SCOPED_SURFACE_ID_ALLOCATOR_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_SCOPED_SURFACE_ID_ALLOCATOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {
class ParentLocalSurfaceIdAllocator;

// While a ScopedSurfaceIdAllocator is alive, it suppresses allocation from the
// ParentLocalSurfaceIdAllocator that was provided to it during construction.
// When the destructor is called, the allocation_task is called. This allows for
// one allocation event to happen during the lifetime of this object.
//
// The default constructor leave that parent allocator invalid and does no
// suppression and doesn't call the allocation_task.
class VIZ_COMMON_EXPORT ScopedSurfaceIdAllocator {
 public:
  explicit ScopedSurfaceIdAllocator(base::OnceCallback<void()> allocation_task);
  explicit ScopedSurfaceIdAllocator(ParentLocalSurfaceIdAllocator* allocator,
                                    base::OnceCallback<void()> allocation_task);

  ScopedSurfaceIdAllocator(const ScopedSurfaceIdAllocator& other) = delete;
  ScopedSurfaceIdAllocator& operator=(const ScopedSurfaceIdAllocator& other) =
      delete;
  ScopedSurfaceIdAllocator(ScopedSurfaceIdAllocator&& other);
  ScopedSurfaceIdAllocator& operator=(ScopedSurfaceIdAllocator&& other);

  ~ScopedSurfaceIdAllocator();

  friend void swap(ScopedSurfaceIdAllocator& first,
                   ScopedSurfaceIdAllocator& second);

 private:
  raw_ptr<ParentLocalSurfaceIdAllocator> allocator_ = nullptr;
  base::OnceCallback<void()> allocation_task_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_SCOPED_SURFACE_ID_ALLOCATOR_H_
