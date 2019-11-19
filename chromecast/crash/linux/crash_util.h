// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_CRASH_UTIL_H_
#define CHROMECAST_CRASH_LINUX_CRASH_UTIL_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"

namespace chromecast {

class CrashUtil {
 public:
  // Helper function to request upload an existing minidump file. Returns true
  // on success, false otherwise.
  static bool RequestUploadCrashDump(const std::string& existing_minidump_path,
                                     uint64_t crashed_pid,
                                     uint64_t crashed_process_start_time_ms);

  // Util function to get current time in ms. This is used to record
  // crashed_process_start_time_ms in client side.
  static uint64_t GetCurrentTimeMs();

  // Call this to set a callback to be used instead of invoking an executable
  // in a seperate process. See MinidumpWriter::SetDumpStateCbForTest() for more
  // details on this callback's signature.
  static void SetDumpStateCbForTest(
      const base::Callback<int(const std::string&)>& cb);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_CRASH_UTIL_H_
