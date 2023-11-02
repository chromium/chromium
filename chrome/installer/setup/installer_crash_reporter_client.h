// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_INSTALLER_CRASH_REPORTER_CLIENT_H_
#define CHROME_INSTALLER_SETUP_INSTALLER_CRASH_REPORTER_CLIENT_H_

#include <stddef.h>

#include "components/crash/core/app/crash_reporter_client.h"

class InstallerCrashReporterClient
    : public crash_reporter::CrashReporterClient {
 public:
  explicit InstallerCrashReporterClient(bool is_per_user_install);

  InstallerCrashReporterClient(const InstallerCrashReporterClient&) = delete;
  InstallerCrashReporterClient& operator=(const InstallerCrashReporterClient&) =
      delete;

  ~InstallerCrashReporterClient() override;

  // crash_reporter::CrashReporterClient methods:
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
  bool GetCollectStatsInSample() override;
  bool ReportingIsEnforcedByPolicy(bool* enabled) override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;

 private:
  bool is_per_user_install_;
};

#endif  // CHROME_INSTALLER_SETUP_INSTALLER_CRASH_REPORTER_CLIENT_H_
