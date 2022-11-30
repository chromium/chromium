// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_CAST_EGL_PLATFORM_SHLIB_H_
#define CHROMECAST_PUBLIC_CAST_EGL_PLATFORM_SHLIB_H_

#include <string>
#include <vector>

#include "chromecast_export.h"

namespace chromecast {

class CastEglPlatform;

// Entry point for loading CastEglPlatform from shared library.
class CHROMECAST_EXPORT CastEglPlatformShlib {
 public:
  static CastEglPlatform* Create(const std::vector<std::string>& argv);
};

}

#endif  // CHROMECAST_PUBLIC_CAST_EGL_PLATFORM_SHLIB_H_
