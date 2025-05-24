// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/process_type.h"

#include "base/command_line.h"
#include "components/sampling_profiler/process_type.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"  // nogncheck
#endif

class ThreadProfilerProcessTypeTest : public ::testing::Test {
 public:
  ThreadProfilerProcessTypeTest()
      : command_line_(base::CommandLine::NO_PROGRAM) {}

  base::CommandLine& command_line() { return command_line_; }

 private:
  base::CommandLine command_line_;
};

TEST_F(ThreadProfilerProcessTypeTest, GetProfilerProcessType_Browser) {
  // No process type switch == browser process.
  EXPECT_EQ(sampling_profiler::ProfilerProcessType::kBrowser,
            GetProfilerProcessType(command_line()));
}

TEST_F(ThreadProfilerProcessTypeTest, GetProfilerProcessType_Renderer) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kRendererProcess);
  EXPECT_EQ(sampling_profiler::ProfilerProcessType::kRenderer,
            GetProfilerProcessType(command_line()));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ThreadProfilerProcessTypeTest, GetProfilerProcessType_Extension) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kRendererProcess);
  command_line().AppendSwitch(extensions::switches::kExtensionProcess);
  // Extension renderers are treated separately from non-extension renderers,
  // but don't have a defined enum currently.
  EXPECT_EQ(sampling_profiler::ProfilerProcessType::kUnknown,
            GetProfilerProcessType(command_line()));
}
#endif

TEST_F(ThreadProfilerProcessTypeTest, GetProfilerProcessType_Gpu) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kGpuProcess);
  EXPECT_EQ(sampling_profiler::ProfilerProcessType::kGpu,
            GetProfilerProcessType(command_line()));
}

TEST_F(ThreadProfilerProcessTypeTest, GetProfilerProcessType_NetworkService) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kUtilityProcess);
  command_line().AppendSwitchASCII(switches::kUtilitySubType,
                                   network::mojom::NetworkService::Name_);
  EXPECT_EQ(sampling_profiler::ProfilerProcessType::kNetworkService,
            GetProfilerProcessType(command_line()));
}

TEST_F(ThreadProfilerProcessTypeTest, GetProfilerProcessType_Utility) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kUtilityProcess);
  EXPECT_EQ(sampling_profiler::ProfilerProcessType::kUtility,
            GetProfilerProcessType(command_line()));
}

TEST_F(ThreadProfilerProcessTypeTest, GetProfilerProcessType_Zygote) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kZygoteProcess);
  EXPECT_EQ(sampling_profiler::ProfilerProcessType::kZygote,
            GetProfilerProcessType(command_line()));
}

TEST_F(ThreadProfilerProcessTypeTest, GetProfilerProcessType_PpapiPlugin) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kPpapiPluginProcess);
  EXPECT_EQ(sampling_profiler::ProfilerProcessType::kPpapiPlugin,
            GetProfilerProcessType(command_line()));
}
