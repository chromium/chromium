// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/crash_util.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/chromecast_buildflags.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/process_utils.h"
#include "chromecast/base/version.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/crash/app_state_tracker.h"
#include "chromecast/crash/linux/dummy_minidump_generator.h"
#include "chromecast/crash/linux/minidump_writer.h"

namespace chromecast {

namespace {
const char kDumpStateSuffix[] = ".txt.gz";
const char kMinidumpsDir[] = "minidumps";
const size_t kMaxFilesInMinidumpsDir = 22;  // 10 crashes

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
// Crash product name for Core web runtime.
constexpr const char kCoreRuntimeCrashProductName[] = "CastCoreRuntime";
#endif  // BUILDFLAG(ENABLE_CAST_RECEIVER)

// This can be set to a callback for testing. This allows us to inject a fake
// dumpstate routine to avoid calling an executable during an automated test.
// This value should not be mutated through any other function except
// CrashUtil::SetDumpStateCbForTest().
static base::OnceCallback<int(const std::string&)>* g_dumpstate_cb = nullptr;

}  // namespace

// static
bool CrashUtil::HasSpaceToCollectCrash() {
  // Normally the path is protected by a file lock, but we don't need to acquire
  // that just to make an estimate of the space consumed.
  base::FilePath dump_dir(GetHomePathASCII(kMinidumpsDir));
  base::FileEnumerator file_enumerator(dump_dir, false /* recursive */,
                                       base::FileEnumerator::FILES);
  size_t total = 0;
  for (base::FilePath name = file_enumerator.Next(); !name.empty();
       name = file_enumerator.Next()) {
    total++;
  }
  return total <= kMaxFilesInMinidumpsDir;
}

// static
bool CrashUtil::CollectDumpstate(const base::FilePath& minidump_path,
                                 base::FilePath* dumpstate_path) {
  std::string dumpstate_bin_path;
  base::FilePath result =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kDumpstateBinPath);

  // In case of CastCore + Chrome runtime, dumpstate path is passed to runtime
  // through command line. If that's missing, then it's likely not a Chromium
  // runtime build and fall back to the default path.
  if (!result.empty()) {
    dumpstate_bin_path = result.value();
  } else {
    LOG(WARNING)
        << "Dumpstate path is missing in command line, using the default path";
    dumpstate_bin_path = chromecast::GetBinPathASCII("dumpstate").value();
  }

  std::vector<std::string> argv = {
      dumpstate_bin_path,   "-w", "crash-request", "-z", "-o",
      minidump_path.value() /* dumpstate appends ".txt.gz" to the filename */
  };

  std::string log;
  if (!chromecast::GetAppOutput(argv, &log)) {
    LOG(ERROR) << "failed to execute dumpstate";
    return false;
  }

  *dumpstate_path = minidump_path.AddExtensionASCII(kDumpStateSuffix);
  return true;
}

// static
uint64_t CrashUtil::GetCurrentTimeMs() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();
}

// static
bool CrashUtil::RequestUploadCrashDump(
    const std::string& existing_minidump_path,
    uint64_t crashed_pid,
    uint64_t crashed_process_start_time_ms,
    const std::vector<Attachment>* attachments) {
  // Remove IO restrictions from this thread. Chromium IO functions must be used
  // to access the file system and upload information to the crash server.
  base::ScopedAllowBlocking allow_blocking;

  LOG(INFO) << "Request to upload crash dump " << existing_minidump_path
            << " for process " << crashed_pid;

  uint64_t uptime_ms = GetCurrentTimeMs() - crashed_process_start_time_ms;
  MinidumpParams params(
      uptime_ms,
      "",  // suffix
      AppStateTracker::GetPreviousApp(), AppStateTracker::GetCurrentApp(),
      AppStateTracker::GetLastLaunchedApp(), CAST_BUILD_RELEASE,
      CAST_BUILD_INCREMENTAL, "", /* reason */
      AppStateTracker::GetStadiaSessionId());

// Set crash product name for Core runtime. If not set, defaults to "Eureka".
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  params.crash_product_name = kCoreRuntimeCrashProductName;
#endif  // BUILDFLAG(ENABLE_CAST_RECEIVER)

  DummyMinidumpGenerator minidump_generator(existing_minidump_path);

  base::FilePath filename = base::FilePath(existing_minidump_path).BaseName();

  std::unique_ptr<MinidumpWriter> writer;
  if (g_dumpstate_cb) {
    writer.reset(new MinidumpWriter(&minidump_generator, filename.value(),
                                    params, std::move(*g_dumpstate_cb),
                                    attachments));
  } else {
    writer.reset(new MinidumpWriter(&minidump_generator, filename.value(),
                                    params, attachments));
  }
  bool success = false;
  success = (0 == writer->Write());  // error already logged.

  // In case the file is still in $TEMP, remove it. Note that DeleteFile() will
  // return true if |existing_minidump_path| has already been moved.
  if (!base::DeleteFile(base::FilePath(existing_minidump_path))) {
    LOG(ERROR) << "Unable to delete temp minidump file "
               << existing_minidump_path;
    success = false;
  }

  // Use std::endl to flush the log stream in case this process exits.
  LOG(INFO) << "Request to upload crash dump finished. "
            << "Exit now if it is main process that crashed." << std::endl;

  return success;
}

void CrashUtil::SetDumpStateCbForTest(
    base::OnceCallback<int(const std::string&)> cb) {
  DCHECK(!g_dumpstate_cb);
  g_dumpstate_cb =
      new base::OnceCallback<int(const std::string&)>(std::move(cb));
}

}  // namespace chromecast
