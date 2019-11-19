// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/sandbox/switches.h"

namespace nacl {

void CopyNaClCommandLineArguments(base::CommandLine* cmd_line) {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();

  // Propagate the following switches to the NaCl loader command line (along
  // with any associated values) if present in the browser command line.
  // TODO(gregoryd): check which flags of those below can be supported.
  static const char* const kSwitchNames[] = {
    service_manager::switches::kNoSandbox,
    switches::kDisableBreakpad,
    switches::kFullMemoryCrashReport,
    switches::kEnableLogging,
    switches::kDisableLogging,
    switches::kLoggingLevel,
    switches::kNoErrorDialogs,
#if defined(OS_MACOSX)
    service_manager::switches::kEnableSandboxLogging,
#endif
#if defined(OS_WIN)
    switches::kDisableHighResTimer,
#endif
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             base::size(kSwitchNames));
}

}  // namespace nacl
