// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/devicemode.h"

#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "ui/ozone/public/ozone_switches.h"

namespace chromeos {

bool IsRunningAsSystemCompositor() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableRunningAsSystemCompositor)) {
    return false;
  }
  static bool is_running_on_chrome_os = base::SysInfo::IsRunningOnChromeOS();
  return is_running_on_chrome_os ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             ::switches::kEnableRunningAsSystemCompositor);
}

}  // namespace chromeos
