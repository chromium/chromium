// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/process_type.h"

#include "base/command_line.h"
#include "components/sampling_profiler/process_type.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"  // nogncheck
#endif

namespace {

// True if the command line corresponds to an extension renderer process.
bool IsExtensionRenderer(const base::CommandLine& command_line) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return command_line.HasSwitch(extensions::switches::kExtensionProcess);
#else
  return false;
#endif
}

}  // namespace

sampling_profiler::ProfilerProcessType GetProfilerProcessType(
    const base::CommandLine& command_line) {
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return sampling_profiler::ProfilerProcessType::kBrowser;

  // Renderer process exclusive of extension renderers.
  if (process_type == switches::kRendererProcess &&
      !IsExtensionRenderer(command_line)) {
    return sampling_profiler::ProfilerProcessType::kRenderer;
  }

  if (process_type == switches::kGpuProcess)
    return sampling_profiler::ProfilerProcessType::kGpu;

  if (process_type == switches::kUtilityProcess) {
    auto utility_sub_type =
        command_line.GetSwitchValueASCII(switches::kUtilitySubType);
    if (utility_sub_type == network::mojom::NetworkService::Name_)
      return sampling_profiler::ProfilerProcessType::kNetworkService;
    return sampling_profiler::ProfilerProcessType::kUtility;
  }

  if (process_type == switches::kZygoteProcess)
    return sampling_profiler::ProfilerProcessType::kZygote;

  if (process_type == switches::kPpapiPluginProcess)
    return sampling_profiler::ProfilerProcessType::kPpapiPlugin;

  return sampling_profiler::ProfilerProcessType::kUnknown;
}
