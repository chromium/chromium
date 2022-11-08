// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_android.h"

#include <memory>

namespace chromecast {

// static
std::unique_ptr<CastSysInfo> CreateSysInfo() {
  return std::make_unique<CastSysInfoAndroid>();
}

}  // namespace chromecast
