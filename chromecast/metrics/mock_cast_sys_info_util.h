// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chromecast/base/cast_sys_info_util.h"

#ifndef CHROMECAST_METRICS_MOCK_CAST_SYS_INFO_UTIL_H_
#define CHROMECAST_METRICS_MOCK_CAST_SYS_INFO_UTIL_H_

namespace chromecast {

int GetSysInfoCreatedCount();
std::unique_ptr<CastSysInfo> CreateSysInfo();

}  // namespace chromecast

#endif  // CHROMECAST_METRICS_MOCK_CAST_SYS_INFO_UTIL_H_