// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/crosier_mixin.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#endif

bool CrosierMixin::SetUpUserDataDirectory() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Always have --user-data-dir present in commandline arguments.
  // Without the argument, there are some permission issues. Here the logic
  // is: a temporary dir is created in base framework and set in PathService.
  // Then we retrieve the dir and append it to the commandline arguments.
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (!cmdline->HasSwitch(switches::kUserDataDir)) {
    cmdline->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }
#endif
  return InProcessBrowserTestMixin::SetUpUserDataDirectory();
}
