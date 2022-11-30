// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chromecast/crash/linux/crash_util.h"
#include "chromecast/external_mojo/external_service_support/crash_reporter_client.h"
#include "components/crash/core/app/breakpad_linux.h"
#include "components/crash/core/app/crash_reporter_client.h"

namespace chromecast {
namespace external_service_support {

namespace {

class CrashReporterBuiltin : public crash_reporter::CrashReporterClient {
 public:
  CrashReporterBuiltin()
      : start_time_ms_(base::TimeTicks::Now().since_origin().InMilliseconds()) {
  }

  ~CrashReporterBuiltin() override = default;

  CrashReporterBuiltin(const CrashReporterBuiltin&) = delete;
  CrashReporterBuiltin& operator=(const CrashReporterBuiltin&) = delete;

  // crash_reporter::CrashReporterClient implementation:
  bool EnableBreakpadForProcess(const std::string& process_type) override {
    return true;
  }
  bool HandleCrashDump(const char* crashdump_filename,
                       uint64_t crash_pid) override {
    chromecast::CrashUtil::RequestUploadCrashDump(crashdump_filename, crash_pid,
                                                  start_time_ms_);
    // Always return true to indicate that this crash dump has been processed,
    // so that it won't fallback to Chrome's default uploader.
    return true;
  }
  bool GetCollectStatsConsent() override {
    // Returning true allows writing the crash dump to disk, but not to
    // upload.  The uploader will check whether the device has opted in to crash
    // uploading.  It would be more optimal to avoid writing the crash dump if
    // the device is opted out, but the complexity of checking that flag would
    // increase the probability of a crash within the crash handler.
    return true;
  }

 private:
  const uint64_t start_time_ms_;
};

CrashReporterBuiltin* GetCrashReporterClient() {
  static base::NoDestructor<CrashReporterBuiltin> crash_reporter_client;
  return crash_reporter_client.get();
}
}  // namespace

// static
void CrashReporterClient::Init() {
#if !defined(OFFICIAL_BUILD)
  base::debug::EnableInProcessStackDumping();
#endif  // !defined(OFFICIAL_BUILD)

  crash_reporter::SetCrashReporterClient(GetCrashReporterClient());
  breakpad::InitCrashReporter(/*process_type=*/"");
}

}  // namespace external_service_support
}  // namespace chromecast
