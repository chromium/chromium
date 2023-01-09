// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_LOGGING_SERVICE_API_H_
#define CHROME_CHROME_CLEANER_LOGGING_LOGGING_SERVICE_API_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/chrome_cleaner/logging/utils.h"
#include "chrome/chrome_cleaner/os/disk_util_types.h"
#include "chrome/chrome_cleaner/os/process.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace base {
class FilePath;
}  // namespace base

namespace chrome_cleaner {

class RegistryLogger;

namespace internal {

struct FileInformation;
struct RegistryValue;

}  // namespace internal

// Manage where the logs are sent, and expose an API for more specific logging.
class LoggingServiceAPI {
 public:
  typedef base::RepeatingCallback<void(bool)> UploadResultCallback;

  virtual ~LoggingServiceAPI() {}

  // Return the singleton instance which will get destroyed by the AtExitMgr.
  // The instance must have been initialized with |Initialize| before
  // being used, and |Terminate| must be called before releasing (or destroying)
  // the object.
  // Tests can force their own logging service implemenation with
  // SetInstanceForTesting().
  static LoggingServiceAPI* GetInstance();

  // Sets the logging service instance for testing purposes only.
  // Using |logging_service| as null resets GetInstance() to its default
  // behaviour.
  // Recommended usage:
  //   void SetUp() override {
  //     LoggingServiceAPI::SetInstance(&my_logging_service_);
  //     LoggingServiceAPI::GetInstance()->Initialize(&my_registry_logger_);
  //   }
  //   void TearDown() override {
  //     LoggingServiceAPI::GetInstance()->Terminate();
  //     LoggingServiceAPI::SetInstance(nullptr);
  //   }
  // Note: when my_logging_service_ is a MockLoggingService, Initialize() and
  //       Terminate() don't need to be called.
  static void SetInstanceForTesting(LoggingServiceAPI* logging_service);

  // All of the following functions must be called from the main UI thread:

  // Start and stop intercepting logs. The function |Terminate| disables uploads
  // and flushes current content of the logging report. |registry_logger| is
  // needed to make sure there are no logs upload scheduled tasks left active
  // when logs upload is disabled. It can be nullptr when caller is positive
  // that there are none, i.e., when initializing for the first time.
  virtual void Initialize(RegistryLogger* registry_logger) = 0;
  virtual void Terminate() = 0;

  // Send the current state of |chrome_cleaner_report_| to the Safe Browsing API
  // and clear it. |registry_logger| is specified by the caller so that it can
  // be mocked for testing.
  virtual void SendLogsToSafeBrowsing(const UploadResultCallback& done_callback,
                                      RegistryLogger* registry_logger) = 0;

  // Cancel all current and future waits, to speed up system shutdown.
  virtual void CancelWaitForShutdown() = 0;

  // Enable / disable uploads of logs. |registry_logger| is needed to make sure
  // there are no logs upload scheduled tasks left active when logs upload is
  // disabled. It can be nullptr when caller is positive that there are none,
  // i.e., when enabling for the first time.
  virtual void EnableUploads(bool enabled, RegistryLogger* registry_logger) = 0;

  // Schedule a task to upload logs in case we fail to progress beyond the point
  // from where this is called, which can be identified by |result_code|. These
  // fall back logs use the current state of raw log lines without flushing
  // them, and use |result_code| as the protobuf exit code.
  virtual void ScheduleFallbackLogsUpload(RegistryLogger* registry_logger,
                                          ResultCode result_code) = 0;

  // All of the following functions can be called from any thread:

  virtual bool uploads_enabled() const = 0;

  // Set |detailed_system_report| to |chrome_cleaner_report_|.
  virtual void SetDetailedSystemReport(bool detailed_system_report) = 0;
  virtual bool detailed_system_report_enabled() const = 0;

  // Add |found_uws_name| to |chrome_cleaner_report_|.
  virtual void AddFoundUwS(const std::string& found_uws_name) = 0;

  // Add |found_uws| to |chrome_cleaner_report_| with the detail level of the
  // UwS set according to |flags|.
  // Can be called from any thread.
  virtual void AddDetectedUwS(const PUPData::PUP* found_uws,
                              UwSDetectedFlags flags) = 0;

  // Adds a converted UwS proto to the list of matched UwS.
  virtual void AddDetectedUwS(const UwS& uws) = 0;

  // Set |exit_code| to |chrome_cleaner_report_|. Can be called from any thread.
  // Must not be called except when really done.
  virtual void SetExitCode(ResultCode exit_code) = 0;

  // Add a loaded module to the system report.
  virtual void AddLoadedModule(
      const std::wstring& name,
      ModuleHost host,
      const internal::FileInformation& file_information) = 0;

  // Add a running service to the system report.
  virtual void AddService(
      const std::wstring& display_name,
      const std::wstring& service_name,
      const internal::FileInformation& file_information) = 0;

  // Add an installed program to the system report.
  virtual void AddInstalledProgram(const base::FilePath& folder_path) = 0;

  // Add a running process to the system report.
  virtual void AddProcess(
      const std::wstring& name,
      const internal::FileInformation& file_information) = 0;

  // Add a registry value |registry_value| which may have |file_informations|
  // associated with it to the system report.
  virtual void AddRegistryValue(
      const internal::RegistryValue& registry_value,
      const std::vector<internal::FileInformation>& file_informations) = 0;

  // Add a layered service provider to the system report.
  virtual void AddLayeredServiceProvider(
      const std::vector<std::wstring>& guids,
      const internal::FileInformation& file_information) = 0;

  // Set the WinInetProxy settings of the system report.
  virtual void SetWinInetProxySettings(const std::wstring& config,
                                       const std::wstring& bypass,
                                       const std::wstring& auto_config_url,
                                       bool autodetect) = 0;

  // Set the WinHttpProxy settings of the system report.
  virtual void SetWinHttpProxySettings(const std::wstring& config,
                                       const std::wstring& bypass) = 0;

  // Add an installed extension to the system report.
  virtual void AddInstalledExtension(
      const std::wstring& extension_id,
      ExtensionInstallMethod install_method,
      const std::vector<internal::FileInformation>& extension_files) = 0;

  // Add a scheduled task to the system report.
  virtual void AddScheduledTask(
      const std::wstring& name,
      const std::wstring& description,
      const std::vector<internal::FileInformation>& actions) = 0;

  // Add a ShortcutData to the system report.
  virtual void AddShortcutData(
      const std::wstring& lnk_path,
      const std::wstring& executable_path,
      const std::string& executable_hash,
      const std::vector<std::wstring>& command_line_arguments) = 0;

  // Set |found_modified_shortcuts| in the |reporter_logs|.
  virtual void SetFoundModifiedChromeShortcuts(
      bool found_modified_shortcuts) = 0;

  // Set |scanned_locations| in the reporter log.
  virtual void SetScannedLocations(
      const std::vector<UwS::TraceLocation>& scanned_locations) = 0;

  // Log resource usage of a Chrome Cleanup process identified by
  // |process_type|.
  virtual void LogProcessInformation(SandboxType process_type,
                                     const SystemResourceUsage& usage) = 0;

  // Returns whether all files detected for removable UwS were successfully
  // deleted.
  virtual bool AllExpectedRemovalsConfirmed() const = 0;

  // Return a raw representation of the current state of the report.
  virtual std::string RawReportContent() = 0;

  // Read the content of |log_file| as a protocol buffer and replace current
  // state with it. Return false on failures.
  virtual bool ReadContentFromFile(const base::FilePath& log_file) = 0;

  // If in debug mode or switch --dump-raw-logs is present, save the serialized
  // report proto to |executable||tag|.pb, where |executable| is the current
  // binary name.
  virtual void MaybeSaveLogsToFile(const std::wstring& tag);

 private:
  static LoggingServiceAPI* logging_service_for_testing_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_LOGGING_SERVICE_API_H_
