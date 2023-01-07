// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/cast_sys_info_shlib.h"

#include "base/command_line.h"
#include "chromecast/base/cast_sys_info_dummy.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/init_command_line_shlib.h"

namespace chromecast {

// static
CastSysInfo* CastSysInfoShlib::Create(const std::vector<std::string>& argv) {
  InitCommandLineShlib(argv);
  auto* cmd_line = base::CommandLine::ForCurrentProcess();

  if (cmd_line->HasSwitch(switches::kSysInfoFilePath)) {
    return new CastSysInfoDummy(
        cmd_line->GetSwitchValueASCII(switches::kSysInfoFilePath));
  }

  return new CastSysInfoDummy();
}

}  // namespace chromecast
