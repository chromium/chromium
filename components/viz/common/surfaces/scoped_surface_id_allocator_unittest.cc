// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"

#include "base/functional/bind.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

// ScopedSurfaceIdAllocator has no accessors which can be used to check
// behavior. ParentLocalSurfaceIdAllocator has the accessors we need:
// - is_allocation_suppressed()
//
// For every operation which changes state we can test:
// - the operation completed as expected,
// - the accessors did not change, and/or
// - the accessors changed in the way we expected.

namespace viz {

// The single argument constructor takes a callback which should be called when
// the ScopedSurfaceIdAllocator goes out of scope.
TEST(ScopedSurfaceIdAllocatorTest,
     SingleArgumentConstructorShouldCallCallback) {
  bool callback_called = false;
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      [](bool* callback_called) { *callback_called = true; }, &callback_called);

  {
    ScopedSurfaceIdAllocator callback_allocator(std::move(allocation_task));

    EXPECT_FALSE(callback_called);
  }
  EXPECT_TRUE(callback_called);
}

// The dual argument constructor takes a ParentLocalSurfaceIdAllocator* and a
// callback. The parent allocator should be suppressed while the
// ScopedSurfaceIdAllocator is alive. When it goes out of scope, the callback
// should be called.
TEST(ScopedSurfaceIdAllocatorTest,
     DualArgumentConstructorShouldSuppressParentAndCallCallback) {
  bool callback_called = false;
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      [](bool* callback_called) { *callback_called = true; }, &callback_called);
  ParentLocalSurfaceIdAllocator parent_allocator;
  EXPECT_FALSE(parent_allocator.is_allocation_suppressed());

  {
    ScopedSurfaceIdAllocator suppressing_allocator(&parent_allocator,
                                                   std::move(allocation_task));

    EXPECT_TRUE(parent_allocator.is_allocation_suppressed());
    EXPECT_FALSE(callback_called);
  }
  EXPECT_TRUE(callback_called);
}

// The move constructor should transfer suppression and callback lifetime.
TEST(ScopedSurfaceIdAllocatorTest, MoveConstructorShouldTransferLifetimes) {
  bool callback_called = false;
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      [](bool* callback_called) { *callback_called = true; }, &callback_called);
  ParentLocalSurfaceIdAllocator parent_allocator;
  EXPECT_FALSE(parent_allocator.is_allocation_suppressed());
  {
    ScopedSurfaceIdAllocator moved_from_allocator(&parent_allocator,
                                                  std::move(allocation_task));
    EXPECT_TRUE(parent_allocator.is_allocation_suppressed());

    {
      ScopedSurfaceIdAllocator moved_to_allocator(
          std::move(moved_from_allocator));

      EXPECT_TRUE(parent_allocator.is_allocation_suppressed());
      EXPECT_FALSE(callback_called);
    }
    EXPECT_FALSE(parent_allocator.is_allocation_suppressed());
    EXPECT_TRUE(callback_called);
  }
}

// The move assignment operator should transfer suppression and callback
// lifetime.
TEST(ScopedSurfaceIdAllocatorTest,
     MoveAssignmentOperatorShouldTransferLifetimes) {
  bool callback_called = false;
  base::OnceCallback<void()> allocation_task = base::BindOnce(
      [](bool* callback_called) { *callback_called = true; }, &callback_called);
  bool second_callback_called = false;
  base::OnceCallback<void()> second_allocation_task = base::BindOnce(
      [](bool* second_callback_called) { *second_callback_called = true; },
      &second_callback_called);
  ParentLocalSurfaceIdAllocator parent_allocator;
  EXPECT_FALSE(parent_allocator.is_allocation_suppressed());
  {
    ScopedSurfaceIdAllocator moved_from_allocator(&parent_allocator,
                                                  std::move(allocation_task));
    EXPECT_TRUE(parent_allocator.is_allocation_suppressed());
    {
      ScopedSurfaceIdAllocator moved_to_allocator(
          std::move(second_allocation_task));
      EXPECT_FALSE(second_callback_called);

      moved_to_allocator = std::move(moved_from_allocator);

      EXPECT_TRUE(parent_allocator.is_allocation_suppressed());
      EXPECT_FALSE(callback_called);
      EXPECT_TRUE(second_callback_called);
    }
    EXPECT_FALSE(parent_allocator.is_allocation_suppressed());
    EXPECT_TRUE(callback_called);
  }
}

}  // namespace viz
