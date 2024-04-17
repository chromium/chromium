// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_CAST_SYS_INFO_SHLIB_H_
#define CHROMECAST_PUBLIC_CAST_SYS_INFO_SHLIB_H_

#include <string>
#include <vector>

#include "chromecast_export.h"

namespace chromecast {

class CastSysInfo;

class CHROMECAST_EXPORT CastSysInfoShlib {
 public:
  // Returns a instance of CastSysInfo for the platform from a shared library.
  // Caller will take ownership of returned pointer.
  static CastSysInfo* Create(const std::vector<std::string>& argv);
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_CAST_SYS_INFO_SHLIB_H_
