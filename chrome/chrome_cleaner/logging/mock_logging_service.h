// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_MOCK_LOGGING_SERVICE_H_
#define CHROME_CHROME_CLEANER_LOGGING_MOCK_LOGGING_SERVICE_H_

#include <string>
#include <vector>

#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/logging/utils.h"
#include "chrome/chrome_cleaner/os/disk_util_types.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_cleaner {

// Mock for the LoggingService API.
class MockLoggingService : public LoggingServiceAPI {
 public:
  MockLoggingService();
  ~MockLoggingService() override;

  // LoggingServiceAPI
  MOCK_METHOD1(Initialize, void(RegistryLogger* registry_logger));
  MOCK_METHOD0(Terminate, void());

  MOCK_METHOD2(SendLogsToSafeBrowsing,
               void(const UploadResultCallback& done_callback,
                    RegistryLogger* registry_logger));
  MOCK_METHOD0(CancelWaitForShutdown, void());
  MOCK_METHOD2(EnableUploads,
               void(bool enabled, RegistryLogger* registry_logger));
  MOCK_CONST_METHOD0(uploads_enabled, bool());
  MOCK_METHOD1(SetDetailedSystemReport, void(bool detailed_system_report));
  MOCK_CONST_METHOD0(detailed_system_report_enabled, bool());
  MOCK_METHOD1(AddFoundUwS, void(const std::string& found_uws_name));
  MOCK_METHOD2(AddDetectedUwS,
               void(const PUPData::PUP* found_uws, UwSDetectedFlags flags));
  MOCK_METHOD1(AddDetectedUwS, void(const UwS& uws));
  MOCK_METHOD1(SetExitCode, void(ResultCode exit_code));
  MOCK_METHOD3(AddLoadedModule,
               void(const std::wstring& name,
                    ModuleHost host,
                    const internal::FileInformation& file_information));
  MOCK_METHOD1(AddInstalledProgram, void(const base::FilePath& folder_path));
  MOCK_METHOD3(AddService,
               void(const std::wstring& display_name,
                    const std::wstring& service_name,
                    const internal::FileInformation& file_information));
  MOCK_METHOD2(AddProcess,
               void(const std::wstring& name,
                    const internal::FileInformation& file_information));
  MOCK_METHOD2(
      AddRegistryValue,
      void(const internal::RegistryValue& registry_value,
           const std::vector<internal::FileInformation>& file_informations));
  MOCK_METHOD2(AddLayeredServiceProvider,
               void(const std::vector<std::wstring>& guids,
                    const internal::FileInformation& file_information));
  MOCK_METHOD4(SetWinInetProxySettings,
               void(const std::wstring& config,
                    const std::wstring& bypass,
                    const std::wstring& auto_config_url,
                    bool autodetect));
  MOCK_METHOD2(SetWinHttpProxySettings,
               void(const std::wstring& config, const std::wstring& bypass));
  MOCK_METHOD3(
      AddInstalledExtension,
      void(const std::wstring& extension_id,
           ExtensionInstallMethod install_method,
           const std::vector<internal::FileInformation>& file_information));
  MOCK_METHOD3(AddScheduledTask,
               void(const std::wstring& name,
                    const std::wstring& description,
                    const std::vector<internal::FileInformation>& actions));
  MOCK_METHOD4(AddShortcutData,
               void(const std::wstring& lnk_path,
                    const std::wstring& executable_path,
                    const std::string& executable_hash,
                    const std::vector<std::wstring>& command_line_arguments));
  MOCK_METHOD1(SetFoundModifiedChromeShortcuts,
               void(bool found_modified_shortcuts));
  MOCK_METHOD1(SetScannedLocations,
               void(const std::vector<UwS::TraceLocation>&));
  MOCK_METHOD2(LogProcessInformation,
               void(SandboxType process_type,
                    const SystemResourceUsage& usage));
  MOCK_CONST_METHOD0(AllExpectedRemovalsConfirmed, bool());
  MOCK_METHOD0(RawReportContent, std::string());
  MOCK_METHOD1(ReadContentFromFile, bool(const base::FilePath& log_file));
  MOCK_METHOD2(ScheduleFallbackLogsUpload,
               void(RegistryLogger* registry_logger, ResultCode result_code));
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_MOCK_LOGGING_SERVICE_H_
