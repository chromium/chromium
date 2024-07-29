// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu.h"
#include "partition_alloc/tagging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_system {

// This test suite contains a check that components_browsertests starts up under
// the correct MTE mode. It's only relevant on Android systems.

class MemorySystemTest : public ::testing::Test {};

TEST_F(MemorySystemTest, VerifyCorrectMTEMode) {
  // Check that components_browsertests starts up in MTE synchronous mode as
  // long as the hardware supports it.
  base::CPU cpu;
  if (cpu.has_mte()) {
    ASSERT_EQ(partition_alloc::internal::GetMemoryTaggingModeForCurrentThread(),
              partition_alloc::TagViolationReportingMode::kSynchronous);
  } else {
    GTEST_SKIP();
  }
}

}  // namespace memory_system
