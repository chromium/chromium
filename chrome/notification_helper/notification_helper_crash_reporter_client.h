// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NOTIFICATION_HELPER_NOTIFICATION_HELPER_CRASH_REPORTER_CLIENT_H_
#define CHROME_NOTIFICATION_HELPER_NOTIFICATION_HELPER_CRASH_REPORTER_CLIENT_H_

#include "base/files/file_path.h"
#include "components/crash/core/app/crash_reporter_client.h"

class NotificationHelperCrashReporterClient
    : public crash_reporter::CrashReporterClient {
 public:
  // Instantiates a process wide instance of the
  // NotificationHelperCrashReporterClient class and initializes crash reporting
  // for the process. The instance is leaked.
  // Uses the crashpad handler embedded in the executable at |exe_path|.
  static void InitializeCrashReportingForProcessWithHandler(
      const base::FilePath& exe_path);

  NotificationHelperCrashReporterClient();

  NotificationHelperCrashReporterClient(
      const NotificationHelperCrashReporterClient&) = delete;
  NotificationHelperCrashReporterClient& operator=(
      const NotificationHelperCrashReporterClient&) = delete;

  ~NotificationHelperCrashReporterClient() override;

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
  bool GetCrashMetricsLocation(std::wstring* metrics_dir) override;
  bool IsRunningUnattended() override;
  bool GetCollectStatsConsent() override;
  bool GetCollectStatsInSample() override;
  bool ReportingIsEnforcedByPolicy(bool* enabled) override;
  bool ShouldMonitorCrashHandlerExpensively() override;
  bool EnableBreakpadForProcess(const std::string& process_type) override;
};

#endif  // CHROME_NOTIFICATION_HELPER_NOTIFICATION_HELPER_CRASH_REPORTER_CLIENT_H_
