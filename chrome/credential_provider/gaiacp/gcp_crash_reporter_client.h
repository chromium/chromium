// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTER_CLIENT_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTER_CLIENT_H_

#include "components/crash/core/app/crash_reporter_client.h"

namespace base {
class FilePath;
}

namespace credential_provider {

class GcpCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  GcpCrashReporterClient() = default;

  GcpCrashReporterClient(const GcpCrashReporterClient&) = delete;
  GcpCrashReporterClient& operator=(const GcpCrashReporterClient&) = delete;

  ~GcpCrashReporterClient() override;

  // crash_reporter::CrashReporterClient:
  bool ShouldCreatePipeName(const std::wstring& process_type) override;
  bool GetAlternativeCrashDumpLocation(std::wstring* crash_dir) override;
  void GetProductNameAndVersion(const std::wstring& exe_path,
                                std::wstring* product_name,
                                std::wstring* version,
                                std::wstring* special_build,
                                std::wstring* channel_name) override;
  bool ShouldShowRestartDialog(std::wstring* title,
                               std::wstring* message,
                               bool* is_rtl_locale) override;
  bool AboutToRestart() override;
  bool GetIsPerUserInstall() override;
  bool GetShouldDumpLargerDumps() override;
  int GetResultCodeRespawnFailed() override;
  bool GetCrashDumpLocation(std::wstring* crash_dir) override;
  bool IsRunningUnattended() override;
  bool GetCollectStatsConsent() override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;

 protected:
  virtual base::FilePath GetPathForFileVersionInfo(
      const std::wstring& exe_path);
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCP_CRASH_REPORTER_CLIENT_H_
