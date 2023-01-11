// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_CRASH_UTIL_H_
#define CHROMECAST_CRASH_LINUX_CRASH_UTIL_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace chromecast {

struct Attachment;

class CrashUtil {
 public:
  // Returns true if the filesystem has sufficient space to collect the crash.
  // Performs I/O on the current thread.
  static bool HasSpaceToCollectCrash();

  // Runs the dumpstate utility and sends its output to a file
  // named after the minidump, but with ".txt.gz" appended to the name.
  // This blocks the current thread to perform I/O (for multiple seconds).
  static bool CollectDumpstate(const base::FilePath& minidump_path,
                               base::FilePath* dumpstate_path);

  // Helper function to request upload an existing minidump file. Returns true
  // on success, false otherwise.
  static bool RequestUploadCrashDump(
      const std::string& existing_minidump_path,
      uint64_t crashed_pid,
      uint64_t crashed_process_start_time_ms,
      const std::vector<Attachment>* attachments = nullptr);

  // Util function to get current time in ms. This is used to record
  // crashed_process_start_time_ms in client side.
  static uint64_t GetCurrentTimeMs();

  // Call this to set a callback to be used instead of invoking an executable
  // in a seperate process. See MinidumpWriter::SetDumpStateCbForTest() for more
  // details on this callback's signature.
  static void SetDumpStateCbForTest(
      base::OnceCallback<int(const std::string&)> cb);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_CRASH_UTIL_H_
