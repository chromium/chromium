// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/switches.h"
#include "base/allocator/buildflags.h"
#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace heap_profiling {

#if BUILDFLAG(USE_ALLOCATOR_SHIM)

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

TEST(HeapProfilingSwitches, GetModeForStartup_Finch) {
  EXPECT_EQ(Mode::kNone, GetModeForStartup());
  std::map<std::string, std::string> parameters;

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = "";
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);

    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = "invalid";
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeAll;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kAll, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeBrowser;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kBrowser, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeMinimal;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kMinimal, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeGpu;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kGpu, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeRendererSampling;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kRendererSampling, GetModeForStartup());
  }
}

// Ensure the commandline overrides any given field trial.
TEST(HeapProfilingSwitches, GetModeForStartup_CommandLinePrecedence) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlogMode,
                                                            kMemlogModeAll);

  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeMinimal;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kOOPHeapProfilingFeature, parameters);

  EXPECT_EQ(Mode::kAll, GetModeForStartup());
}

#else

TEST(HeapProfilingSwitches, GetModeForStartup_NoModeWithoutShim) {
  {
    base::test::ScopedCommandLine scoped_command_line;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(kMemlogMode,
                                                              kMemlogModeAll);
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> parameters;
    parameters[kOOPHeapProfilingFeatureMode] = kMemlogModeMinimal;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kOOPHeapProfilingFeature, parameters);
    EXPECT_EQ(Mode::kNone, GetModeForStartup());
  }
}

#endif

}  // namespace heap_profiling
