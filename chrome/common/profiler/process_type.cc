// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/process_type.h"

#include "base/command_line.h"
#include "components/metrics/call_stacks/call_stack_profile_params.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"
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

metrics::CallStackProfileParams::Process GetProfileParamsProcess(
    const base::CommandLine& command_line) {
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty())
    return metrics::CallStackProfileParams::Process::kBrowser;

  // Renderer process exclusive of extension renderers.
  if (process_type == switches::kRendererProcess &&
      !IsExtensionRenderer(command_line)) {
    return metrics::CallStackProfileParams::Process::kRenderer;
  }

  if (process_type == switches::kGpuProcess)
    return metrics::CallStackProfileParams::Process::kGpu;

  if (process_type == switches::kUtilityProcess) {
    auto utility_sub_type =
        command_line.GetSwitchValueASCII(switches::kUtilitySubType);
    if (utility_sub_type == network::mojom::NetworkService::Name_)
      return metrics::CallStackProfileParams::Process::kNetworkService;
    return metrics::CallStackProfileParams::Process::kUtility;
  }

  if (process_type == switches::kZygoteProcess)
    return metrics::CallStackProfileParams::Process::kZygote;

  if (process_type == switches::kPpapiPluginProcess)
    return metrics::CallStackProfileParams::Process::kPpapiPlugin;

  return metrics::CallStackProfileParams::Process::kUnknown;
}
