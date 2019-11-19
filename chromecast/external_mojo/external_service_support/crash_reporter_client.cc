// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromecast/external_mojo/external_service_support/crash_reporter_client.h"

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chromecast/crash/linux/crash_util.h"
#include "components/crash/content/app/breakpad_linux.h"

namespace chromecast {
namespace external_service_support {

namespace {
CrashReporterClient* GetCrashReporterClient() {
  static base::NoDestructor<CrashReporterClient> crash_reporter_client;
  return crash_reporter_client.get();
}
}  // namespace

// static
void CrashReporterClient::InitCrashReporter() {
  crash_reporter::SetCrashReporterClient(GetCrashReporterClient());
  breakpad::InitCrashReporter(/*process_type=*/"");
}

CrashReporterClient::CrashReporterClient()
    : start_time_ms_(
          (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds()) {}

CrashReporterClient::~CrashReporterClient() = default;

bool CrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return true;
}

bool CrashReporterClient::HandleCrashDump(const char* crashdump_filename,
                                          uint64_t crash_pid) {
  chromecast::CrashUtil::RequestUploadCrashDump(crashdump_filename, crash_pid,
                                                start_time_ms_);

  // Always return true to indicate that this crash dump has been processed,
  // so that it won't fallback to Chrome's default uploader.
  return true;
}

bool CrashReporterClient::GetCollectStatsConsent() {
  // Returning true allows writing the crash dump to disk, but not to
  // upload.  The uploader will check whether the device has opted in to crash
  // uploading.  It would be more optimal to avoid writing the crash dump if the
  // device is opted out, but the complexity of checking that flag would
  // increase the probability of a crash within the crash handler.
  return true;
}

}  // namespace external_service_support
}  // namespace chromecast
