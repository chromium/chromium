// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_APP_ANDROID_CAST_CRASH_REPORTER_CLIENT_ANDROID_H_
#define CHROMECAST_APP_ANDROID_CAST_CRASH_REPORTER_CLIENT_ANDROID_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "components/crash/core/app/crash_reporter_client.h"

namespace chromecast {

class CastCrashReporterClientAndroid
    : public crash_reporter::CrashReporterClient {
 public:
  explicit CastCrashReporterClientAndroid(const std::string& process_type);

  CastCrashReporterClientAndroid(const CastCrashReporterClientAndroid&) =
      delete;
  CastCrashReporterClientAndroid& operator=(
      const CastCrashReporterClientAndroid&) = delete;

  ~CastCrashReporterClientAndroid() override;

  // Return the path to a directory of MIME-encoded crash reports.
  static bool GetCrashReportsLocation(const std::string& process_type,
                                      base::FilePath* crash_dir);

  // crash_reporter::CrashReporterClient implementation:
  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override;
  base::FilePath GetReporterLogFilename() override;
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;
  int GetAndroidMinidumpDescriptor() override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;

 private:
  std::string process_type_;
};

}  // namespace chromecast

#endif  // CHROMECAST_APP_ANDROID_CAST_CRASH_REPORTER_CLIENT_ANDROID_H_
