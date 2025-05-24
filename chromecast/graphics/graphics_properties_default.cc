// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/immediate_crash.h"
#include "chromecast/public/graphics_properties_shlib.h"

namespace chromecast {

bool GraphicsPropertiesShlib::IsSupported(
    Resolution resolution,
    const std::vector<std::string>& argv) {
  switch (resolution) {
    case Resolution::k1080p:
      return std::ranges::any_of(argv, [](const std::string& arg) {
        // This is defined by `kDesktopWindow1080p`, but it can't be used here
        // since //chromecast/base depends on //base.
        return arg == "-desktop-window-1080p" ||
               arg == "--desktop-window-1080p";
      });
    case Resolution::kUHDTV:
      return false;
    default:
      base::ImmediateCrash();
  }
}

}  // namespace chromecast
