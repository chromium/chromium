// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/init_command_line_shlib.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chromecast/chromecast_buildflags.h"

namespace chromecast {


void InitCommandLineShlib(const std::vector<std::string>& argv) {
  if (base::CommandLine::InitializedForCurrentProcess())
    return;

  base::CommandLine::Init(0, nullptr);
  base::CommandLine::ForCurrentProcess()->InitFromArgv(argv);

  logging::InitLogging(logging::LoggingSettings());
#if BUILDFLAG(IS_CAST_DESKTOP_BUILD)
  logging::SetLogItems(true, true, true, false);
#else
  // Timestamp available through logcat -v time.
  logging::SetLogItems(true, true, false, false);
#endif  // BUILDFLAG(IS_CAST_DESKTOP_BUILD)
}

}  // namespace chromecast
