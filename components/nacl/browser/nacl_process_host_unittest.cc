// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "components/nacl/browser/nacl_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

TEST(NaClProcessHostTest, AddressSpaceAllocation) {
  size_t size = 1 << 20;  // 1 MB
  void* addr = nacl::AllocateAddressSpaceASLR(GetCurrentProcess(), size);
  bool success = VirtualFree(addr, 0, MEM_RELEASE);
  ASSERT_TRUE(success);
}
#endif
