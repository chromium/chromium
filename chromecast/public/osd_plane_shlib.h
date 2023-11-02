// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_OSD_PLANE_SHLIB_H_
#define CHROMECAST_PUBLIC_OSD_PLANE_SHLIB_H_

#include <string>
#include <vector>

#include "chromecast_export.h"

namespace chromecast {

class OsdPlane;

// Entry point for loading OsdPlane from shared library.
class CHROMECAST_EXPORT OsdPlaneShlib {
 public:
  static OsdPlane* Create(const std::vector<std::string>& argv);
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_OSD_PLANE_SHLIB_H_
