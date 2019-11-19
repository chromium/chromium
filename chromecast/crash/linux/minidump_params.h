// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_MINIDUMP_PARAMS_H_
#define CHROMECAST_CRASH_LINUX_MINIDUMP_PARAMS_H_

#include <stdint.h>

#include <string>

namespace chromecast {

struct MinidumpParams {
  MinidumpParams();
  MinidumpParams(const uint64_t p_process_uptime,
                 const std::string& p_suffix,
                 const std::string& p_previous_app_name,
                 const std::string& p_current_app_name,
                 const std::string& p_last_app_name,
                 const std::string& p_cast_release_version,
                 const std::string& p_cast_build_number,
                 const std::string& p_reason);
  MinidumpParams(const MinidumpParams& params);
  ~MinidumpParams();

  uint64_t process_uptime;
  std::string suffix;
  std::string previous_app_name;
  std::string current_app_name;
  std::string last_app_name;
  // Release version is in the format of "major.minor", such as "1.15".
  std::string cast_release_version;
  // Build number is numerical string such as "20000".
  std::string cast_build_number;
  // Reason for crash, if one is available.
  std::string reason;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_MINIDUMP_PARAMS_H_
