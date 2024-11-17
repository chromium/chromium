// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/fallback_crash_handler_win.h"

#include <dbghelp.h>
#include <psapi.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "build/build_config.h"
#include "components/crash/core/app/minidump_with_crashpad_info.h"

namespace crash_reporter {

namespace {

void AcquireMemoryMetrics(const base::Process& process,
                          StringStringMap* crash_keys) {
  // Grab the process private memory.
  // This is best effort, though really shouldn't ever fail.
  PROCESS_MEMORY_COUNTERS_EX process_memory = {sizeof(process_memory)};
  if (GetProcessMemoryInfo(
          process.Handle(),
          reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process_memory),
          sizeof(process_memory))) {
    // This is in units of bytes, re-scale to pages for consistency with
    // system metrics.
    const uint64_t kPageSize = 4096;
    crash_keys->insert(std::make_pair(
        "ProcessPrivateUsage",
        base::NumberToString(process_memory.PrivateUsage / kPageSize)));

    crash_keys->insert(std::make_pair(
        "ProcessPeakWorkingSetSize",
        base::NumberToString(process_memory.PeakWorkingSetSize / kPageSize)));

    crash_keys->insert(std::make_pair(
        "ProcessPeakPagefileUsage",
        base::NumberToString(process_memory.PeakPagefileUsage / kPageSize)));
  }

  // Grab system commit memory. Also best effort.
  PERFORMANCE_INFORMATION perf_info = {sizeof(perf_info)};
  if (GetPerformanceInfo(&perf_info, sizeof(perf_info))) {
    // Record the remaining committable memory and the limit. This is in units
    // of system pages.
    crash_keys->insert(std::make_pair(
        "SystemCommitRemaining",
        base::NumberToString(perf_info.CommitLimit - perf_info.CommitTotal)));
    crash_keys->insert(std::make_pair(
        "SystemCommitLimit", base::NumberToString(perf_info.CommitLimit)));
  }
}

}  // namespace

FallbackCrashHandler::FallbackCrashHandler()
    : thread_id_(base::kInvalidThreadId), exception_ptrs_(0UL) {}

FallbackCrashHandler::~FallbackCrashHandler() = default;

bool FallbackCrashHandler::ParseCommandLine(const base::CommandLine& cmd_line) {
  // Retrieve the handle to the process to dump.
  unsigned int uint_process;
  if (!base::StringToUint(cmd_line.GetSwitchValueASCII("process"),
                          &uint_process)) {
    return false;
  }

  // Before taking ownership of the supposed handle, see whether it's really
  // a process handle.
  base::ProcessHandle process_handle = base::win::Uint32ToHandle(uint_process);
  if (base::GetProcId(process_handle) == base::kNullProcessId)
    return false;

  // Retrieve the thread id argument.
  unsigned int thread_id = 0;
  if (!base::StringToUint(cmd_line.GetSwitchValueASCII("thread"), &thread_id)) {
    return false;
  }
  thread_id_ = thread_id;

  // Retrieve the "exception-pointers" argument.
  uint64_t uint_exc_ptrs = 0;
  if (!base::StringToUint64(cmd_line.GetSwitchValueASCII("exception-pointers"),
                            &uint_exc_ptrs)) {
    return false;
  }
  exception_ptrs_ = static_cast<uintptr_t>(uint_exc_ptrs);

  // Retrieve the "database" argument.
  database_dir_ = cmd_line.GetSwitchValuePath("database");
  if (database_dir_.empty())
    return false;

  // Everything checks out, take ownership of the process handle.
  process_ = base::Process(process_handle);

  return true;
}

bool FallbackCrashHandler::GenerateCrashDump(const std::string& product,
                                             const std::string& version,
                                             const std::string& channel,
                                             const std::string& process_type) {
  MINIDUMP_EXCEPTION_INFORMATION exc_info = {};
  exc_info.ThreadId = thread_id_;
  exc_info.ExceptionPointers =
      reinterpret_cast<EXCEPTION_POINTERS*>(exception_ptrs_);
  exc_info.ClientPointers = TRUE;  // ExceptionPointers in client.

// Mandatory crash keys. These will be read by Crashpad and used as
// http request parameters for the upload. Keys and values need to match
// server side configuration.
#if defined(ARCH_CPU_64_BITS)
  const char* platform = "Win64";
#else
  const char* platform = "Win32";
#endif
  std::map<std::string, std::string> crash_keys = {{"prod", product},
                                                   {"ver", version},
                                                   {"channel", channel},
                                                   {"plat", platform},
                                                   {"ptype", process_type}};

  // Add memory metrics relating to system-wide and target process memory usage.
  AcquireMemoryMetrics(process_, &crash_keys);

  uint32_t minidump_type = MiniDumpWithUnloadedModules |
                           MiniDumpWithProcessThreadData |
                           MiniDumpWithFullMemoryInfo |
                           MiniDumpWithThreadInfo;

  // Capture more detail for canary and dev channels. The prefix search caters
  // for the legacy "-m" suffixed multi-install channels.
  if (channel.find("canary") == 0 || channel.find("dev") == 0)
    minidump_type |= MiniDumpWithIndirectlyReferencedMemory;

  return DumpAndReportProcess(process_, minidump_type, &exc_info, crash_keys,
                              database_dir_);
}

}  // namespace crash_reporter
