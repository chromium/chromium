// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/mock_cast_sys_info_util.h"

#include "chromecast/base/cast_sys_info_dummy.h"

namespace chromecast {

static int times_sys_info_created_ = 0;

int GetSysInfoCreatedCount() {
  return times_sys_info_created_;
}

std::unique_ptr<CastSysInfo> CreateSysInfo() {
  times_sys_info_created_ += 1;
  return std::make_unique<CastSysInfoDummy>();
}

}  // namespace chromecast