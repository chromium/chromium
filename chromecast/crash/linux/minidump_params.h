// Copyright 2015 The Chromium Authors
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
                 const std::string& p_reason,
                 const std::string& p_stadia_session_id,
                 const std::string& p_extra_info = "",
                 const std::string& p_exec_name = "",
                 const std::string& p_signature = "",
                 const std::string& p_crash_product_name = "",
                 const std::string& p_comments = "",
                 const std::string& p_js_engine = "",
                 const std::string& p_js_build_label = "",
                 const std::string& p_js_exception_category = "",
                 const std::string& p_js_exception_details = "",
                 const std::string& p_js_exception_signature = "");
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
  // Stadia Session ID, if a Stadia session was running at the time of crash.
  std::string stadia_session_id;
  std::string extra_info;
  std::string exec_name;
  std::string signature;
  // Crash Product name, used to identify/group crash reports in go/crash.
  std::string crash_product_name;

  // CastLite specific crash report data
  std::string comments;
  std::string js_engine;
  std::string js_build_label;
  std::string js_exception_category;
  std::string js_exception_details;
  std::string js_exception_signature;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_MINIDUMP_PARAMS_H_
