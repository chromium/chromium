// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_NOOP_LOGGING_SERVICE_H_
#define CHROME_CHROME_CLEANER_LOGGING_NOOP_LOGGING_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace chrome_cleaner {

// Manage where the logs are sent, and expose an API for more specific logging.
class NoOpLoggingService : public LoggingServiceAPI {
 public:
  // Return the singleton instance which will get destroyed by the AtExitMgr.
  static NoOpLoggingService* GetInstance();

  NoOpLoggingService(const NoOpLoggingService&) = delete;
  NoOpLoggingService& operator=(const NoOpLoggingService&) = delete;

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
      const std::wstring& name,
      ModuleHost host,
      const internal::FileInformation& file_information) override;
  void AddInstalledProgram(const base::FilePath& folder_path) override;
  void AddService(const std::wstring& display_name,
                  const std::wstring& service_name,
                  const internal::FileInformation& file_information) override;
  void AddProcess(const std::wstring& name,
                  const internal::FileInformation& file_information) override;
  void AddRegistryValue(
      const internal::RegistryValue& registry_value,
      const std::vector<internal::FileInformation>& file_informations) override;
  void AddLayeredServiceProvider(
      const std::vector<std::wstring>& guids,
      const internal::FileInformation& file_information) override;
  void SetWinInetProxySettings(const std::wstring& config,
                               const std::wstring& bypass,
                               const std::wstring& auto_config_url,
                               bool autodetect) override;
  void SetWinHttpProxySettings(const std::wstring& config,
                               const std::wstring& bypass) override;
  void AddInstalledExtension(
      const std::wstring& extension_id,
      ExtensionInstallMethod install_method,
      const std::vector<internal::FileInformation>& extension_files) override;
  void AddScheduledTask(
      const std::wstring& name,
      const std::wstring& description,
      const std::vector<internal::FileInformation>& actions) override;

  void AddShortcutData(
      const std::wstring& lnk_path,
      const std::wstring& executable_path,
      const std::string& executable_hash,
      const std::vector<std::wstring>& command_line_arguments) override;
  void SetFoundModifiedChromeShortcuts(bool found_modified_shortcuts) override;
  void SetScannedLocations(const std::vector<UwS::TraceLocation>&) override;

  void LogProcessInformation(SandboxType process_type,
                             const SystemResourceUsage& usage) override;

  bool AllExpectedRemovalsConfirmed() const override;
  std::string RawReportContent() override;
  bool ReadContentFromFile(const base::FilePath& log_file) override;
  void ScheduleFallbackLogsUpload(RegistryLogger* registry_logger,
                                  ResultCode result_code) override;

 private:
  friend struct base::DefaultSingletonTraits<NoOpLoggingService>;
  NoOpLoggingService();
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_NOOP_LOGGING_SERVICE_H_
