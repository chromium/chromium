// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_FALLBACK_CRASH_HANDLER_WIN_H_
#define COMPONENTS_CRASH_CORE_APP_FALLBACK_CRASH_HANDLER_WIN_H_

#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/threading/platform_thread.h"

namespace crash_reporter {

// In the fallback crash handler, this invokes the system crash machinery
// (MiniDumpWriteDump) to generate the crash report, then adds the report to
// the Crashpad database for upload.
class FallbackCrashHandler {
 public:
  FallbackCrashHandler();

  FallbackCrashHandler(const FallbackCrashHandler&) = delete;
  FallbackCrashHandler& operator=(const FallbackCrashHandler&) = delete;

  ~FallbackCrashHandler();

  // Parses |cmd_line| for the following arguments:
  // --database=<path>
  //   Path to the Crashpad database where the minidump will be stored.
  // --exception-pointers=<address>
  //   Address of exception pointers in the crashed process.
  // --process=<handle>
  //   Handle to the process to dump.
  // --thread=<id>
  //   ID of the crashing thread.
  bool ParseCommandLine(const base::CommandLine& cmd_line);

  // Generates a crashdump with Crashpad properties containing:
  // prod: |product|
  // ver: |version|
  // channel: |cannel|
  // plat: "Win32" or "Win64", depending on bitness.
  // ptype: |process_type|.
  bool GenerateCrashDump(const std::string& product,
                         const std::string& version,
                         const std::string& channel,
                         const std::string& process_type);

  const base::Process& process() const { return process_; }

 private:
  base::Process process_;
  base::PlatformThreadId thread_id_;
  // This is a pointer in process_, which is hopefully not this process.
  uintptr_t exception_ptrs_;
  base::FilePath database_dir_;
};

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_FALLBACK_CRASH_HANDLER_WIN_H_
