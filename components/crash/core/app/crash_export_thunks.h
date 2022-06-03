// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_CRASH_EXPORT_THUNKS_H_
#define COMPONENTS_CRASH_CORE_APP_CRASH_EXPORT_THUNKS_H_

#include <stddef.h>
#include <time.h>
#include <windows.h>

#include "build/build_config.h"

namespace crash_reporter {
struct Report;
}

extern "C" {

// All the functions in this file are named with a suffix of _ExportThunk to
// make sure their names cannot easily collide with random other functions.
// The Microsoft Visual Studio linker has a misfeature where it searches rather
// aggressively for functions to match exports in .DEF files.
// See https://crbug.com/760385#c11 for how this can be bad.

// This function may be invoked across module boundaries to request a single
// crash report upload. See CrashUploadListCrashpad.
void RequestSingleCrashUpload_ExportThunk(const char* local_id);

// This function may be invoked across module boundaries to retrieve the crash
// list. It copies up to |report_count| reports into |reports| and returns the
// number of reports available. If the return value is less than or equal to
// |reports_size|, then |reports| contains all the available reports.
size_t GetCrashReports_ExportThunk(crash_reporter::Report* reports,
                                   size_t reports_size);

// Crashes the process after generating a dump for the provided exception. Note
// that the crash reporter should be initialized before calling this function
// for it to do anything.
// NOTE: This function is used by SyzyASAN to invoke a crash. If you change the
// the name or signature of this function you will break SyzyASAN instrumented
// releases of Chrome. Please contact syzygy-team@chromium.org before doing so!
int CrashForException_ExportThunk(EXCEPTION_POINTERS* info);

// This function is used in chrome_metrics_services_manager_client.cc to trigger
// changes to the upload-enabled state. This is done when the metrics services
// are initialized, and when the user changes their consent for uploads. See
// crash_reporter::SetUploadConsent for effects. The given consent value should
// be consistent with
// crash_reporter::GetCrashReporterClient()->GetCollectStatsConsent(), but it's
// not enforced to avoid blocking startup code on synchronizing them.
void SetUploadConsent_ExportThunk(bool consent);

// Injects a thread into a remote process to dump state when there is no crash.
// |process| that represents serialized crash keys sent from the browser.
// This method is used solely to classify hung input.
HANDLE InjectDumpForHungInput_ExportThunk(HANDLE process);

// Returns the crashpad database path.
const wchar_t* GetCrashpadDatabasePath_ExportThunk();

// This function may be invoked across module boundaries to delete reports
// within the time range. See crash_reporter::ClearReportsBetween.
void ClearReportsBetween_ExportThunk(time_t begin, time_t end);

// Immediately dump |process| to a crash dump adorned with |ptype|.
// Takes ownership of |process|, does not kill nor affect the exit code of
// |process|.
bool DumpHungProcessWithPtype_ExportThunk(HANDLE process, const char* ptype);

}  // extern "C"

#endif  // COMPONENTS_CRASH_CORE_APP_CRASH_EXPORT_THUNKS_H_
