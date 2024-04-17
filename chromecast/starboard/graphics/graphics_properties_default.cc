// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/graphics_properties_shlib.h"

namespace chromecast {

bool GraphicsPropertiesShlib::IsSupported(
    Resolution resolution,
    const std::vector<std::string>& argv) {
  switch (resolution) {
    case Resolution::k1080p:
      return true;
    case Resolution::kUHDTV:
      return false;
    default:
      return false;
  }
}

}  // namespace chromecast
