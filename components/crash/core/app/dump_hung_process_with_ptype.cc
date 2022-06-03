// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/minidump_with_crashpad_info.h"

#include "base/files/file_util.h"
#include "components/crash/core/app/crash_export_thunks.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "components/crash/core/app/crashpad.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/minidump/minidump_extensions.h"

namespace crash_reporter {

bool DumpHungProcessWithPtype(const base::Process& process, const char* ptype) {
  // We need to pass a handle across the export thunk, which takes ownership
  // of it.
  HANDLE process_handle = nullptr;
  if (!::DuplicateHandle(::GetCurrentProcess(), process.Handle(),
                         ::GetCurrentProcess(), &process_handle, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    return false;
  }

  return DumpHungProcessWithPtype_ExportThunk(process_handle, ptype);
}

bool DumpHungProcessWithPtypeImpl(const base::Process& process,
                                  const char* ptype) {
  // Get annotations on the crash report to set the product name, version,
  // channel and so forth to what's appropriate for this process.
  StringStringMap annotations;
  internal::GetPlatformCrashpadAnnotations(&annotations);

  // Override the process type, as this is dumping |process| rather than this
  // process.
  annotations["ptype"] = ptype;

  uint32_t minidump_type = MiniDumpWithUnloadedModules |
                           MiniDumpWithProcessThreadData |
                           MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo;

  // Capture more detail for canary and dev channels. The prefix search caters
  // for the legacy "-m" suffixed multi-install channels.
  std::string channel_name = annotations["channel"];
  if (channel_name.find("canary") == 0 || channel_name.find("dev") == 0)
    minidump_type |= MiniDumpWithIndirectlyReferencedMemory;

  return DumpAndReportProcess(process, minidump_type, nullptr, annotations,
                              crash_reporter::GetCrashpadDatabasePath());
}

}  // namespace crash_reporter
