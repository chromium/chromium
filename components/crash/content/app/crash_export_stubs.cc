// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file has no-op stub implementation for the functions declared in
// crash_export_thunks.h. This is solely for linking into tests, where
// crash reporting is unwanted and never initialized.

#include <windows.h>

#include "build/build_config.h"
#include "components/crash/content/app/crash_export_thunks.h"
#include "components/crash/content/app/crashpad.h"

void RequestSingleCrashUpload_ExportThunk(const char* local_id) {}

size_t GetCrashReports_ExportThunk(crash_reporter::Report* reports,
                                   size_t reports_size) {
  return 0;
}

int CrashForException_ExportThunk(EXCEPTION_POINTERS* info) {
  // Make sure to properly crash the process by dispatching directly to the
  // Windows unhandled exception filter.
  return UnhandledExceptionFilter(info);
}

void SetUploadConsent_ExportThunk(bool consent) {}

HANDLE InjectDumpForHungInput_ExportThunk(HANDLE process) {
  return nullptr;
}

const wchar_t* GetCrashpadDatabasePath_ExportThunk() {
  return nullptr;
}

void ClearReportsBetween_ExportThunk(time_t begin, time_t end) {}

bool DumpHungProcessWithPtype_ExportThunk(HANDLE process_handle,
                                          const char* ptype) {
  return false;
}
