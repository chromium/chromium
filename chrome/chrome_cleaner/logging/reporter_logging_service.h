// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_REPORTER_LOGGING_SERVICE_H_
#define CHROME_CHROME_CLEANER_LOGGING_REPORTER_LOGGING_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chrome/chrome_cleaner/logging/detailed_info_sampler.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/proto/reporter_logs.pb.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/logging/safe_browsing_reporter.h"
#include "chrome/chrome_cleaner/os/disk_util_types.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// Manage where the logs are sent, and expose an API for more specific logging.
class ReporterLoggingService : public LoggingServiceAPI {
 public:
  // Return the singleton instance which will get destroyed by the AtExitMgr.
  static ReporterLoggingService* GetInstance();

  // LoggingServiceAPI.
  void Initialize(RegistryLogger* registry_logger) override;
  void Terminate() override;

  void SendLogsToSafeBrowsing(const UploadResultCallback& done_callback,
                              RegistryLogger* registry_logger) override;
  void CancelWaitForShutdown() override;
  void EnableUploads(bool enabled, RegistryLogger* registry_logger) override;
  bool uploads_enabled() const override;
  void SetDetailedSystemReport(bool detailed_system_report) override;
  bool detailed_system_report_enabled() const override;
  void AddFoundUwS(const std::string& found_uws_name) override;
  void AddDetectedUwS(const PUPData::PUP* found_uws,
                      UwSDetectedFlags flags) override;
  void AddDetectedUwS(const UwS& uws) override;
  void SetExitCode(ResultCode exit_code) override;
  void AddLoadedModule(
      const base::string16& name,
      ModuleHost host,
      const internal::FileInformation& file_information) override;
  void AddInstalledProgram(const base::FilePath& folder_path) override;
  void AddService(const base::string16& display_name,
                  const base::string16& service_name,
                  const internal::FileInformation& file_information) override;
  void AddProcess(const base::string16& name,
                  const internal::FileInformation& file_information) override;
  void AddRegistryValue(
      const internal::RegistryValue& registry_value,
      const std::vector<internal::FileInformation>& file_informations) override;
  void AddLayeredServiceProvider(
      const std::vector<base::string16>& guids,
      const internal::FileInformation& file_information) override;
  void SetWinInetProxySettings(const base::string16& config,
                               const base::string16& bypass,
                               const base::string16& auto_config_url,
                               bool autodetect) override;
  void SetWinHttpProxySettings(const base::string16& config,
                               const base::string16& bypass) override;
  void AddInstalledExtension(
      const base::string16& extension_id,
      ExtensionInstallMethod install_method,
      const std::vector<internal::FileInformation>& extension_files) override;
  void AddScheduledTask(
      const base::string16& name,
      const base::string16& description,
      const std::vector<internal::FileInformation>& actions) override;

  void AddShortcutData(
      const base::string16& lnk_path,
      const base::string16& executable_path,
      const std::string& executable_hash,
      const std::vector<base::string16>& command_line_arguments) override;

  void SetFoundModifiedChromeShortcuts(bool found_modified_shortcuts) override;

  void SetScannedLocations(
      const std::vector<UwS::TraceLocation>& scanned_locations) override;

  void LogProcessInformation(SandboxType process_type,
                             const SystemResourceUsage& usage) override;

  bool AllExpectedRemovalsConfirmed() const override;
  std::string RawReportContent() override;
  bool ReadContentFromFile(const base::FilePath& log_file) override;
  void ScheduleFallbackLogsUpload(RegistryLogger* registry_logger,
                                  ResultCode result_code) override;

 private:
  friend struct base::DefaultSingletonTraits<ReporterLoggingService>;

  ReporterLoggingService();
  ~ReporterLoggingService() override;

  // Return true if |reporter_logs_|'s values have changed since it has
  // been cleared.
  bool IsReportingNeeded() const;

  // Callback for |safe_browsing_reporter_|.
  void OnReportUploadResult(const UploadResultCallback& done_callback,
                            RegistryLogger* registry_logger,
                            SafeBrowsingReporter::Result result,
                            const std::string& serialized_report,
                            std::unique_ptr<ChromeFoilResponse> response);

  // Any access to |reporter_logs_| must be protected by |lock_|. While
  // under this lock, no outside function calls should be made, and no logging
  // (this includes DCHECK).
  mutable base::Lock lock_;
  FoilReporterLogs reporter_logs_;

  // Whether the logging service has been initialized.
  bool initialized_ = false;

  // Default to false, so EnableUploads must be called to set it to true.
  bool uploads_enabled_ = false;

  // |uploads_enabled_| must only be set from the thread that created the
  // ReporterLoggingService.
  THREAD_CHECKER(thread_checker_);

  // Sampler to choose which files to log detailed info for.
  DetailedInfoSampler sampler_;

  DISALLOW_COPY_AND_ASSIGN(ReporterLoggingService);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_REPORTER_LOGGING_SERVICE_H_
