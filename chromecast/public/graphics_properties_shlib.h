// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_GRAPHICS_PROPERTIES_SHLIB_H_
#define CHROMECAST_PUBLIC_GRAPHICS_PROPERTIES_SHLIB_H_

#include <string>
#include <vector>

#include "chromecast_export.h"

namespace chromecast {

class CHROMECAST_EXPORT GraphicsPropertiesShlib {
 public:
  // Optional resolutions that cast_shell queries for.  720p (1280x720) is
  // assumed to be supported.
  enum Resolution {
    k1080p,  // 1920x1080
    kUHDTV   // 3840x2160
  };

  // Returns whether or not the given display resolution is supported.
  // Called in the browser process; command line args are provided.
  static bool IsSupported(Resolution resolution,
                          const std::vector<std::string>& argv);
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_GRAPHICS_PROPERTIES_SHLIB_H_
