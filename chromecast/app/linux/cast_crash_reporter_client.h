// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_APP_LINUX_CAST_CRASH_REPORTER_CLIENT_H_
#define CHROMECAST_APP_LINUX_CAST_CRASH_REPORTER_CLIENT_H_

#include <stdint.h>

#include <string>

#include "components/crash/core/app/crash_reporter_client.h"

namespace chromecast {

class CastCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  static void InitCrashReporter(const std::string& process_type);

  CastCrashReporterClient();

  CastCrashReporterClient(const CastCrashReporterClient&) = delete;
  CastCrashReporterClient& operator=(const CastCrashReporterClient&) = delete;

  ~CastCrashReporterClient() override;

  // crash_reporter::CrashReporterClient implementation:
  bool EnableBreakpadForProcess(const std::string& process_type) override;
  bool HandleCrashDump(const char* crashdump_filename,
                       uint64_t crash_pid) override;

 private:
  static uint64_t GetProcessStartTime();
};

}  // namespace chromecast

#endif  // CHROMECAST_APP_LINUX_CAST_CRASH_REPORTER_CLIENT_H_
