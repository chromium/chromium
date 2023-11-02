// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chromecast/base/cast_sys_info_util.h"

#include "chromecast/base/cast_sys_info_dummy.h"

namespace chromecast {

// static
std::unique_ptr<CastSysInfo> CreateSysInfo() {
  return std::make_unique<CastSysInfoDummy>();
}

}  // namespace chromecast
