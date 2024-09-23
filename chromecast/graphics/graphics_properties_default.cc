// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/notreached.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/init_command_line_shlib.h"
#include "chromecast/public/graphics_properties_shlib.h"

namespace chromecast {

bool GraphicsPropertiesShlib::IsSupported(
    Resolution resolution,
    const std::vector<std::string>& argv) {
  InitCommandLineShlib(argv);
  switch (resolution) {
    case Resolution::k1080p:
      return base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDesktopWindow1080p);
    case Resolution::kUHDTV:
      return false;
    default:
      NOTREACHED();
  }
}

}  // namespace chromecast
