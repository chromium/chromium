// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_CLEANER_LOGGING_SERVICE_H_
#define CHROME_CHROME_CLEANER_LOGGING_CLEANER_LOGGING_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chrome/chrome_cleaner/logging/detailed_info_sampler.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/message_builder.h"
#include "chrome/chrome_cleaner/logging/proto/chrome_cleaner_report.pb.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/logging/safe_browsing_reporter.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// Auxiliary functions to convert Cleaner proto messages to their string
// equivalent and append the result to a MessageBuilder.
void AppendMatchedFile(const MatchedFile& file, MessageBuilder* builder);
void AppendFolderInformation(const FolderInformation& folder,
                             MessageBuilder* builder);
void AppendMatchedRegistryEntry(const MatchedRegistryEntry& registry,
                                MessageBuilder* builder);

// Return the enumerator corresponding to the value of the chrome prompt flag
// as set on the command line. Used to set the cleaner_startup field in the
// cleaner logs. Exposed for testing.
ChromeCleanerReport::CleanerStartup GetCleanerStartupFromCommandLine(
    const base::CommandLine* command_line);

// Manage where the logs are sent, and expose an API for more specific logging.
class CleanerLoggingService : public LoggingServiceAPI {
 public:
  // Return the singleton instance which will get destroyed by the AtExitMgr.
  static CleanerLoggingService* GetInstance();

  // LoggingServiceAPI:
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
  void AddInstalledExtension(const base::string16& extension_id,
                             ExtensionInstallMethod install_method) override;
  void AddScheduledTask(
      const base::string16& name,
      const base::string16& description,
      const std::vector<internal::FileInformation>& actions) override;
  void LogProcessInformation(SandboxType process_type,
                             const SystemResourceUsage& usage) override;

  bool AllExpectedRemovalsConfirmed() const override;

  std::string RawReportContent() override;
  bool ReadContentFromFile(const base::FilePath& log_file) override;
  void ScheduleFallbackLogsUpload(RegistryLogger* registry_logger,
                                  ResultCode result_code) override;

 private:
  friend struct base::DefaultSingletonTraits<CleanerLoggingService>;

  CleanerLoggingService();
  ~CleanerLoggingService() override;

  // Callback for |safe_browsing_reporter_|.
  void OnReportUploadResult(const UploadResultCallback& done_callback,
                            RegistryLogger* registry_logger,
                            SafeBrowsingReporter::Result result,
                            const std::string& serialized_report,
                            std::unique_ptr<ChromeFoilResponse> response);

  // Return true if |chrome_cleaner_report_|'s values have changed since it has
  // been cleared.
  bool IsReportingNeeded() const;

  // Clears the temporary log file and it's associated scheduled task.
  void ClearTempLogFile(RegistryLogger* registry_logger);

  // Callback for logging::SetLogMessageHandler.
  static bool LogMessageHandlerFunction(int severity,
                                        const char* file,
                                        int line,
                                        size_t message_start,
                                        const std::string& str);

  // Returns a copy of |chrome_cleaner_report_| in |chrome_cleaner_report|, with
  // an updated Client ID.
  void GetCurrentChromeCleanerReport(
      ChromeCleanerReport* chrome_cleaner_report);

  // Adds all files and folder paths to the corresponding FileInformation and
  // FolderInformation objects. Expects the lock to be held by the caller.
  void UpdateMatchedFilesAndFoldersMaps(UwS* added_uws);

  // Reads the removal and quarantine status of all files and folders from
  // FileRemovalStatusUpdater and updates them in the report.
  void UpdateFileRemovalStatuses();

  // Cache of the strings extracted from the proper locale resource.
  mutable std::map<uint32_t, base::string16> resource_strings_cache_;

  // Any access to |chrome_cleaner_report_|, |matched_files_|, and
  // |matched_folders_| must be protected by |lock_|. While under this lock, no
  // outside function calls and no logging (this includes DCHECK) should be
  // made. Trying to log while under this lock will result in a deadlock, since
  // adding log lines to our raw_log_lines field requires acquiring the lock,
  // and we do not allow reentrancy.
  mutable base::Lock lock_;
  ChromeCleanerReport chrome_cleaner_report_;
  // Map files and folder names to the corresponding MatchedFile and
  // MatchedFolder objects, to allow updates after paths are collected by
  // the scanner (e.g. to update removal status according to information given
  // by the cleaner). Each file path is associated with a vector in case it's
  // matched for more than one UwS.
  std::unordered_map<std::string, std::vector<MatchedFile*>> matched_files_;
  std::unordered_map<std::string, std::vector<MatchedFolder*>> matched_folders_;

  // Saves raw log lines that will be uploaded in the cleaner report.
  std::vector<std::string> raw_log_lines_buffer_;
  mutable base::Lock raw_log_lines_buffer_lock_;

  // |uploads_enabled| must only be accessed from the thread that created the
  // CleanerLoggingService.
  THREAD_CHECKER(thread_checker_);

  // The path to the temporary log file to retry uploading in case we fail.
  // Set as we register a task to retry the logs upload and cleared if another
  // one is scheduled at a later stage and when the logs upload succeeds.
  base::FilePath temp_log_file_;

  // Default to false, so EnableUploads must be called to set it to true.
  bool uploads_enabled_;

  // Whether the logging service has been initialized.
  bool initialized_;

  // Sampler to choose which files to log detailed info for.
  DetailedInfoSampler sampler_;

  DISALLOW_COPY_AND_ASSIGN(CleanerLoggingService);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_CLEANER_LOGGING_SERVICE_H_
