// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_LAUNCH_PREFETCH_APP_LAUNCH_PREFETCH_H_
#define COMPONENTS_APP_LAUNCH_PREFETCH_APP_LAUNCH_PREFETCH_H_

#include "base/command_line.h"
#include "base/component_export.h"

namespace app_launch_prefetch {

// These are the App Launch PreFetch (ALPF) splits we do based on process
// type and subtype to differentiate file and offset usage within the files
// accessed by the different variations of the browser processes with the
// same process name.
enum class SubprocessType {
  kBrowser,
  kBrowserBackground,
  kCatchAll,
  kCrashpad,
  kCrashpadFallback,
  kExtension,
  kGPU,
  kGPUInfo,
  kPpapi,
  kRenderer,
  kUtilityAudio,
  kUtilityNetworkService,
  kUtilityStorage,
  kUtilityOther
};

// Returns an argument to be added to a command line when launching a process to
// direct the Windows prefetcher to use the profile indicated by
// `prefetch_type`.
COMPONENT_EXPORT(APP_LAUNCH_PREFETCH)
base::CommandLine::StringViewType GetPrefetchSwitch(SubprocessType type);

}  // namespace app_launch_prefetch

#endif  // COMPONENTS_APP_LAUNCH_PREFETCH_APP_LAUNCH_PREFETCH_H_
