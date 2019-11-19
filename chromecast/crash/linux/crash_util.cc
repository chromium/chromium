// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/crash_util.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/version.h"
#include "chromecast/crash/app_state_tracker.h"
#include "chromecast/crash/linux/dummy_minidump_generator.h"
#include "chromecast/crash/linux/minidump_writer.h"

namespace chromecast {

namespace {

// This can be set to a callback for testing. This allows us to inject a fake
// dumpstate routine to avoid calling an executable during an automated test.
// This value should not be mutated through any other function except
// CrashUtil::SetDumpStateCbForTest().
static base::Callback<int(const std::string&)>* g_dumpstate_cb = nullptr;

}  // namespace

// static
uint64_t CrashUtil::GetCurrentTimeMs() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();
}

// static
bool CrashUtil::RequestUploadCrashDump(
    const std::string& existing_minidump_path,
    uint64_t crashed_pid,
    uint64_t crashed_process_start_time_ms) {
  // Remove IO restrictions from this thread. Chromium IO functions must be used
  // to access the file system and upload information to the crash server.
  const bool io_allowed = base::ThreadRestrictions::SetIOAllowed(true);

  LOG(INFO) << "Request to upload crash dump " << existing_minidump_path
            << " for process " << crashed_pid;

  uint64_t uptime_ms = GetCurrentTimeMs() - crashed_process_start_time_ms;
  MinidumpParams params(
      uptime_ms,
      "",  // suffix
      AppStateTracker::GetPreviousApp(), AppStateTracker::GetCurrentApp(),
      AppStateTracker::GetLastLaunchedApp(), CAST_BUILD_RELEASE,
      CAST_BUILD_INCREMENTAL, "" /* reason */);
  DummyMinidumpGenerator minidump_generator(existing_minidump_path);

  base::FilePath filename = base::FilePath(existing_minidump_path).BaseName();

  std::unique_ptr<MinidumpWriter> writer;
  if (g_dumpstate_cb) {
    writer.reset(new MinidumpWriter(
        &minidump_generator, filename.value(), params, *g_dumpstate_cb));
  } else {
    writer.reset(
        new MinidumpWriter(&minidump_generator, filename.value(), params));
  }
  bool success = false;
  success = (0 == writer->Write());  // error already logged.

  // In case the file is still in $TEMP, remove it. Note that DeleteFile() will
  // return true if |existing_minidump_path| has already been moved.
  if (!base::DeleteFile(base::FilePath(existing_minidump_path), false)) {
    LOG(ERROR) << "Unable to delete temp minidump file "
               << existing_minidump_path;
    success = false;
  }

  // Use std::endl to flush the log stream in case this process exits.
  LOG(INFO) << "Request to upload crash dump finished. "
            << "Exit now if it is main process that crashed." << std::endl;

  // Restore the original IO restrictions on the thread, if there were any.
  base::ThreadRestrictions::SetIOAllowed(io_allowed);

  return success;
}

void CrashUtil::SetDumpStateCbForTest(
    const base::Callback<int(const std::string&)>& cb) {
  DCHECK(!g_dumpstate_cb);
  g_dumpstate_cb = new base::Callback<int(const std::string&)>(cb);
}

}  // namespace chromecast
