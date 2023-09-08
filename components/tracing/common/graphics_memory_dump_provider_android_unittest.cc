// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/graphics_memory_dump_provider_android.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/traced_value.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

using testing::Contains;
using testing::Eq;
using testing::ByRef;
using base::trace_event::MemoryAllocatorDump;

TEST(GraphicsMemoryDumpProviderTest, ParseResponse) {
  const char* kDumpBaseName = GraphicsMemoryDumpProvider::kDumpBaseName;

  base::trace_event::ProcessMemoryDump pmd(
      {base::trace_event::MemoryDumpLevelOfDetail::kDetailed});
  auto* instance = GraphicsMemoryDumpProvider::GetInstance();
  char buf[] = "graphics_total 12\ngraphics_pss 34\ngl_total 56\ngl_pss 78";
  instance->ParseResponseAndAddToDump(buf, strlen(buf), &pmd);

  {
    // Check the "graphics" row.
    auto* mad = pmd.GetAllocatorDump(kDumpBaseName + std::string("graphics"));
    ASSERT_TRUE(mad);
    MemoryAllocatorDump::Entry total("memtrack_total", "bytes", 12);
    MemoryAllocatorDump::Entry pss("memtrack_pss", "bytes", 34);
    ASSERT_THAT(mad->entries(), Contains(Eq(ByRef(total))));
    ASSERT_THAT(mad->entries(), Contains(Eq(ByRef(pss))));
  }

  {
    // Check the "gl" row.
    auto* mad = pmd.GetAllocatorDump(kDumpBaseName + std::string("gl"));
    ASSERT_TRUE(mad);
    MemoryAllocatorDump::Entry total("memtrack_total", "bytes", 56);
    MemoryAllocatorDump::Entry pss("memtrack_pss", "bytes", 78);
    ASSERT_THAT(mad->entries(), Contains(Eq(ByRef(total))));
    ASSERT_THAT(mad->entries(), Contains(Eq(ByRef(pss))));
  }

  // Test for truncated input.
  pmd.Clear();
  instance->ParseResponseAndAddToDump(buf, strlen(buf) - 14, &pmd);
  ASSERT_TRUE(pmd.GetAllocatorDump(kDumpBaseName + std::string("graphics")));
  ASSERT_FALSE(pmd.GetAllocatorDump(kDumpBaseName + std::string("gl")));
}

}  // namespace tracing
