// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/switches.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "partition_alloc/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace heap_profiling {

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

TEST(HeapProfilingSwitches, GetModeForStartup_Default) {
  EXPECT_EQ(Mode::kNone, GetModeForStartup());
}

TEST(HeapProfilingSwitches, GetModeForStartup_Commandline) {
  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlogMode, "");
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlogMode,
                                                              "invalid");
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlogMode,
                                                              kMemlogModeAll);
    EXPECT_EQ(Mode::kAll, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kMemlogMode, kMemlogModeBrowser);
    EXPECT_EQ(Mode::kBrowser, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kMemlogMode, kMemlogModeMinimal);
    EXPECT_EQ(Mode::kMinimal, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlogMode,
                                                              kMemlogModeGpu);
    EXPECT_EQ(Mode::kGpu, GetModeForStartup());
  }

  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kMemlogMode, kMemlogModeRendererSampling);
    EXPECT_EQ(Mode::kRendererSampling, GetModeForStartup());
  }
}

#else

TEST(HeapProfilingSwitches, GetModeForStartup_NoModeWithoutShim) {
  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlogMode,
                                                              kMemlogModeAll);
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }
}

#endif

}  // namespace heap_profiling
