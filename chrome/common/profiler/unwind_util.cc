// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/unwind_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/common/profiler/process_type.h"
#include "components/metrics/call_stack_profile_params.h"

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
#include "chrome/android/modules/stack_unwinder/public/module.h"
#endif

// static
void UnwindPrerequisites::RequestInstallation() {
  CHECK_EQ(metrics::CallStackProfileParams::Process::kBrowser,
           GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess()));
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
  // The install occurs asynchronously, with the module available at the first
  // run of Chrome following install.
  stack_unwinder::Module::RequestInstallation();
#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
}

// static
bool UnwindPrerequisites::Available() {
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
  return stack_unwinder::Module::IsInstalled();
#else
  return true;
#endif
}
