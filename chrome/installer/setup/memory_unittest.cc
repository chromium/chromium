// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory.h"

#include <stddef.h>

#include <limits>

#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/allocator/allocator_interception_mac.h"
#endif

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
// Test that the allocator shim is in-place so that base::UncheckedMalloc works.
TEST(OutOfMemoryHandledTest, UncheckedMalloc) {
  // Enable termination on OOM - just as setup.exe does at early initialization
  // - and test that UncheckedMalloc properly by-passes this in order to allow
  // the caller to handle OOM.
  base::EnableTerminationOnOutOfMemory();

  const size_t kSafeMallocSize = 512;

  void* value = nullptr;
  EXPECT_TRUE(base::UncheckedMalloc(kSafeMallocSize, &value));
  EXPECT_NE(nullptr, value);
  free(value);

  // Make test size as large as possible minus a few pages so that alignment or
  // other rounding doesn't make it wrap.
  const size_t kUnsafeMallocSize(std::numeric_limits<std::size_t>::max() -
                                 12 * 1024);

  EXPECT_FALSE(base::UncheckedMalloc(kUnsafeMallocSize, &value));
  EXPECT_EQ(nullptr, value);

#if BUILDFLAG(IS_MAC)
  base::allocator::UninterceptMallocZonesForTesting();
#endif
}
#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
