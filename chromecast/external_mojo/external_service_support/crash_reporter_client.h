// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_CRASH_REPORTER_CLIENT_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_CRASH_REPORTER_CLIENT_H_

#include <cstdint>
#include <string>

#include "base/macros.h"
#include "components/crash/content/app/crash_reporter_client.h"

namespace chromecast {
namespace external_service_support {

class CrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  CrashReporterClient();
  ~CrashReporterClient() override;

  static void InitCrashReporter();

  // crash_reporter::CrashReporterClient implementation:
  bool EnableBreakpadForProcess(const std::string& process_type) override;
  bool HandleCrashDump(const char* crashdump_filename,
                       uint64_t crash_pid) override;
  bool GetCollectStatsConsent() override;

 private:
  const uint64_t start_time_ms_;

  DISALLOW_COPY_AND_ASSIGN(CrashReporterClient);
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_CRASH_REPORTER_CLIENT_H_
