// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_APP_LINUX_CAST_CRASH_REPORTER_CLIENT_H_
#define CHROMECAST_APP_LINUX_CAST_CRASH_REPORTER_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "components/crash/content/app/crash_reporter_client.h"

namespace chromecast {

class CastCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  static void InitCrashReporter(const std::string& process_type);

  CastCrashReporterClient();
  ~CastCrashReporterClient() override;

  // crash_reporter::CrashReporterClient implementation:
  bool EnableBreakpadForProcess(const std::string& process_type) override;
  bool HandleCrashDump(const char* crashdump_filename,
                       uint64_t crash_pid) override;

 private:
  static uint64_t GetProcessStartTime();

  DISALLOW_COPY_AND_ASSIGN(CastCrashReporterClient);
};

}  // namespace chromecast

#endif  // CHROMECAST_APP_LINUX_CAST_CRASH_REPORTER_CLIENT_H_
