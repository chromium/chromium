// Copyright 2015 The Chromium Authors
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
                               const std::string& p_reason,
                               const std::string& p_stadia_session_id,
                               const std::string& p_extra_info,
                               const std::string& p_exec_name,
                               const std::string& p_signature,
                               const std::string& p_crash_product_name,
                               const std::string& p_comments,
                               const std::string& p_js_engine,
                               const std::string& p_js_build_label,
                               const std::string& p_js_exception_category,
                               const std::string& p_js_exception_details,
                               const std::string& p_js_exception_signature)
    : process_uptime(p_process_uptime),
      suffix(p_suffix),
      previous_app_name(p_previous_app_name),
      current_app_name(p_current_app_name),
      last_app_name(p_last_app_name),
      cast_release_version(p_cast_release_version),
      cast_build_number(p_cast_build_number),
      reason(p_reason),
      stadia_session_id(p_stadia_session_id),
      extra_info(p_extra_info),
      exec_name(p_exec_name),
      signature(p_signature),
      crash_product_name(p_crash_product_name),
      comments(p_comments),
      js_engine(p_js_engine),
      js_build_label(p_js_build_label),
      js_exception_category(p_js_exception_category),
      js_exception_details(p_js_exception_details),
      js_exception_signature(p_js_exception_signature) {}

MinidumpParams::MinidumpParams() : process_uptime(0) {}

MinidumpParams::MinidumpParams(const MinidumpParams& params) = default;

MinidumpParams::~MinidumpParams() {
}

}  // namespace chromecast
