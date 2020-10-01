// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/process_type.h"

#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "sandbox/policy/sandbox.h"

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
    return metrics::CallStackProfileParams::BROWSER_PROCESS;

  // Renderer process exclusive of extension renderers.
  if (process_type == switches::kRendererProcess &&
      !IsExtensionRenderer(command_line)) {
    return metrics::CallStackProfileParams::RENDERER_PROCESS;
  }

  if (process_type == switches::kGpuProcess)
    return metrics::CallStackProfileParams::GPU_PROCESS;

  if (process_type == switches::kUtilityProcess) {
    auto sandbox_type =
        sandbox::policy::SandboxTypeFromCommandLine(command_line);
    if (sandbox_type == sandbox::policy::SandboxType::kNetwork)
      return metrics::CallStackProfileParams::NETWORK_SERVICE_PROCESS;
    return metrics::CallStackProfileParams::UTILITY_PROCESS;
  }

  if (process_type == switches::kZygoteProcess)
    return metrics::CallStackProfileParams::ZYGOTE_PROCESS;

  if (process_type == switches::kPpapiPluginProcess)
    return metrics::CallStackProfileParams::PPAPI_PLUGIN_PROCESS;

  if (process_type == switches::kPpapiBrokerProcess)
    return metrics::CallStackProfileParams::PPAPI_BROKER_PROCESS;

  return metrics::CallStackProfileParams::UNKNOWN_PROCESS;
}
