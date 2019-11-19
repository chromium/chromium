// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/minidump_params.h"

namespace chromecast {

MinidumpParams::MinidumpParams(const uint64_t p_process_uptime,
                               const std::string& p_suffix,
                               const std::string& p_previous_app_name,
                               const std::string& p_current_app_name,
                               const std::string& p_last_app_name,
                               const std::string& p_cast_release_version,
                               const std::string& p_cast_build_number,
                               const std::string& p_reason)
    : process_uptime(p_process_uptime),
      suffix(p_suffix),
      previous_app_name(p_previous_app_name),
      current_app_name(p_current_app_name),
      last_app_name(p_last_app_name),
      cast_release_version(p_cast_release_version),
      cast_build_number(p_cast_build_number),
      reason(p_reason) {}

MinidumpParams::MinidumpParams() : process_uptime(0) {}

MinidumpParams::MinidumpParams(const MinidumpParams& params) = default;

MinidumpParams::~MinidumpParams() {
}

}  // namespace chromecast
