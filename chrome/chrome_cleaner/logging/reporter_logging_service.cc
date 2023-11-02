// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/reporter_logging_service.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/i18n.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/chrome_utils/chrome_util.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "chrome/chrome_cleaner/logging/api_keys.h"
#include "chrome/chrome_cleaner/logging/noop_logging_service.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/logging/utils.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/settings/settings.h"

namespace chrome_cleaner {

namespace {
// TODO(joenotcharles): Refer to the report definition in the "data" section.
constexpr net::NetworkTrafficAnnotationTag kReporterTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("unwanted_software_report", R"(
          semantics {
            sender: "Chrome Cleanup"
            description:
              "Chrome on Windows is able to detect and remove software that "
              "violates Google's Unwanted Software Policy "
              "(https://www.google.com/about/unwanted-software-policy.html). "
              "When potentially unwanted software is detected in the "
              "background, Chrome may upload information about the detected "
              "software to Google, if the user has opted in to automatically "
              "send system information to help detect dangerous apps and "
              "sites. "
              "The user can also use the settings page to ask Chrome to search "
              "for unwanted software and remove it. In this case if the user "
              "chooses \"Report details to Google\", information about "
              "unwanted software that is detected can be uploaded to Google "
              "even if the user has not opted in to automatically send system "
              "information to help detect dangerous apps and sites."
            trigger:
              "Chrome detected the presence of unwanted software on the system."
            data:
              "The user's Chrome version, Windows version, and locale, file "
              "metadata and system settings linked to the unwanted software "
              "that was detected as described at "
              "https://www.google.com/chrome/privacy/whitepaper.html#unwantedsoftware. "
              "Contents of files are never reported. No user identifiers are "
              "reported, and common user identifiers found in metadata are "
              "replaced with generic strings, but it is possible some metadata "
              "may contain personally identifiable information. This "
              "information is a subset of the information in "
              "\"chrome_cleanup_report\". The complete data specification is "
              "at "
              "https://cs.chromium.org/chromium/src/chrome/chrome_cleaner/logging/proto/reporter_logs.proto."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Reporting of unwanted software that is detected automatically "
              "in the background can be disabled by unchecking \"Automatically "
              "send some system information and page content to Google to help "
              "detect dangerous apps and sites\" in the \"Privacy and "
              "Security\" section of settings, under Advanced. This does not "
              "disable detection of unwanted software, only the reports to "
              "Google. "
              "Chrome Cleanup can also be explicitly requested by the user in "
              "\"Reset and clean up\" in settings under Advanced. To disable "
              "this report, turn off \"Report details to Google\" before "
              "choosing \"Find harmful software\"."
            chrome_policy {
              SafeBrowsingExtendedReportingEnabled {
                SafeBrowsingExtendedReportingEnabled: false
              }
              ChromeCleanupReportingEnabled {
                ChromeCleanupReportingEnabled: false
              }
              ChromeCleanupEnabled  {
                ChromeCleanupEnabled: false
              }
            }
          }
          comments:
            "If ChromeCleanupEnabled is set to \"false\", Chrome Cleanup will "
            "never run, so no reports will be uploaded to Google. Otherwise "
            "ChromeCleanupReportingEnabled can override the \"Report details "
            "to Google\" control and the \"Automatically send some system "
            "information and page content to Google to help detect dangerous "
            "apps and sites\" setting: if it is set to \"true\", reports will "
            "always be sent, and if it is set to \"false\", reports will never "
            "be sent. If ChromeCleanupReportingEnabled is unset, "
            "SafeBrowsingExtendedReportingEnabled can override the "
            "\"Automatically send some system information and page content to "
            "Google to help detect dangerous apps and sites\" setting, but not "
            "the \"Report details to Google\" control."
          )");
}  // namespace

ReporterLoggingService* ReporterLoggingService::GetInstance() {
  return base::Singleton<ReporterLoggingService>::get();
}

ReporterLoggingService::ReporterLoggingService()
    : sampler_(DetailedInfoSampler::kDefaultMaxFiles) {}

ReporterLoggingService::~ReporterLoggingService() {
  DCHECK(!initialized_)
      << "'Terminate' must be called before deleting ReporterLoggingService.";
}

void ReporterLoggingService::Initialize(RegistryLogger* registry_logger) {
  DCHECK(!initialized_) << "LoggingService already initialized.";

  std::vector<std::wstring> languages;
  base::win::i18n::GetUserPreferredUILanguageList(&languages);
  std::wstring version_string;
  bool version_string_succeeded =
      RetrieveChromeVersionAndInstalledDomain(&version_string, nullptr);
  int channel = 0;
  bool has_chrome_channel = GetChromeChannelFromCommandLine(&channel);
  Settings* settings = Settings::GetInstance();
  std::wstring session_id = settings->session_id();
  const Engine::Name engine = settings->engine();
  const std::string engine_version = settings->engine_version();

  {
    base::AutoLock lock(lock_);

    // Ensure that logging report starts in a cleared state.
    reporter_logs_.Clear();

    if (!session_id.empty())
      reporter_logs_.set_session_id(base::WideToUTF8(session_id));

    // If we upload logs for this run, use pending if no exit code is provided
    // elsewhere.
    reporter_logs_.set_exit_code(RESULT_CODE_PENDING);

    // Set invariant environment / machine data.
    FoilReporterLogs_EnvironmentData* env_data =
        reporter_logs_.mutable_environment();
    env_data->set_windows_version(static_cast<int>(base::win::GetVersion()));
    env_data->set_reporter_version(CHROME_CLEANER_VERSION_UTF8_STRING);
    if (version_string_succeeded)
      env_data->set_chrome_version(base::WideToUTF8(version_string));
    if (has_chrome_channel)
      env_data->set_chrome_channel(channel);
    if (languages.size() > 0)
      env_data->set_default_locale(base::WideToUTF8(languages[0]));
    env_data->set_bitness(IsX64Process() ? 64 : 32);
    env_data->mutable_engine()->set_name(engine);
    if (!engine_version.empty())
      env_data->mutable_engine()->set_version(engine_version);

    initialized_ = true;
  }
}

void ReporterLoggingService::Terminate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(initialized_) << "Logging service is not initialized.";
  {
    base::AutoLock lock(lock_);
    initialized_ = false;
    uploads_enabled_ = false;
  }
}

void ReporterLoggingService::SendLogsToSafeBrowsing(
    const UploadResultCallback& done_callback,
    RegistryLogger* registry_logger) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(initialized_);

  // If reporting is not enabled or required, call |done_callback|.
  if (!(uploads_enabled_ && IsReportingNeeded()))
    return done_callback.Run(false);  // false since no logs were uploaded.

  SafeBrowsingReporter::UploadReport(
      base::BindRepeating(&ReporterLoggingService::OnReportUploadResult,
                          base::Unretained(this), done_callback,
                          registry_logger),
      kSafeBrowsingReporterUrl, RawReportContent(), kReporterTrafficAnnotation);
}

void ReporterLoggingService::CancelWaitForShutdown() {}

void ReporterLoggingService::EnableUploads(bool enabled,
                                           RegistryLogger* registry_logger) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  uploads_enabled_ = enabled;
}

bool ReporterLoggingService::uploads_enabled() const {
  return uploads_enabled_;
}

void ReporterLoggingService::SetDetailedSystemReport(
    bool /*detailed_system_report*/) {}

bool ReporterLoggingService::detailed_system_report_enabled() const {
  return false;
}

void ReporterLoggingService::AddFoundUwS(
    const std::string& /*found_uws_name*/) {}

void ReporterLoggingService::AddDetectedUwS(const PUPData::PUP* found_uws,
                                            UwSDetectedFlags flags) {
  DCHECK(found_uws);
  UwS detected_uws =
      PUPToUwS(found_uws, flags, /*cleaning_files=*/false, &sampler_);
  AddDetectedUwS(detected_uws);
}

void ReporterLoggingService::AddDetectedUwS(const UwS& uws) {
  base::AutoLock lock(lock_);
  *reporter_logs_.add_detected_uws() = uws;
}

void ReporterLoggingService::SetExitCode(ResultCode exit_code) {
  ResultCode previous_exit_code = RESULT_CODE_INVALID;
  {
    base::AutoLock lock(lock_);
    previous_exit_code = static_cast<ResultCode>(reporter_logs_.exit_code());
    reporter_logs_.set_exit_code(exit_code);
  }
  // The DCHECK can't be under |lock_|. The only valid reason to overwrite a non
  // pending exit code, is when we failed to read pending upload log files.
  // Since pending logs upload is not implemented by the reporter yet, we will
  // only guarantee that we don't override the exit code in the logs proto.
  DCHECK(previous_exit_code == RESULT_CODE_PENDING);
}

void ReporterLoggingService::AddLoadedModule(
    const std::wstring& /*name*/,
    ModuleHost /*host*/,
    const internal::FileInformation& /*file_information*/) {}

void ReporterLoggingService::AddService(
    const std::wstring& /*display_name*/,
    const std::wstring& /*service_name*/,
    const internal::FileInformation& /*file_information*/) {}

void ReporterLoggingService::AddInstalledProgram(
    const base::FilePath& /*folder_path*/) {}

void ReporterLoggingService::AddProcess(
    const std::wstring& /*name*/,
    const internal::FileInformation& /*file_information*/) {}

void ReporterLoggingService::AddRegistryValue(
    const internal::RegistryValue& /*registry_value*/,
    const std::vector<internal::FileInformation>& /*file_informations*/) {}

void ReporterLoggingService::AddLayeredServiceProvider(
    const std::vector<std::wstring>& /*guids*/,
    const internal::FileInformation& /*file_information*/) {}

void ReporterLoggingService::SetWinInetProxySettings(
    const std::wstring& /*config*/,
    const std::wstring& /*bypass*/,
    const std::wstring& /*auto_config_url*/,
    bool /*autodetect*/) {}

void ReporterLoggingService::SetWinHttpProxySettings(
    const std::wstring& /*config*/,
    const std::wstring& /*bypass*/) {}

void ReporterLoggingService::AddInstalledExtension(
    const std::wstring& extension_id,
    ExtensionInstallMethod install_method,
    const std::vector<internal::FileInformation>& extension_files) {}

void ReporterLoggingService::AddScheduledTask(
    const std::wstring& /*name*/,
    const std::wstring& /*description*/,
    const std::vector<internal::FileInformation>& /*actions*/) {}

void ReporterLoggingService::AddShortcutData(
    const std::wstring& /*lnk_path*/,
    const std::wstring& /*executable_path*/,
    const std::string& /*executable _hash*/,
    const std::vector<std::wstring>& /*command_line_arguments*/) {}

void ReporterLoggingService::SetFoundModifiedChromeShortcuts(
    bool found_modified_shortcuts) {
  base::AutoLock lock(lock_);
  reporter_logs_.set_found_modified_chrome_shortcuts(found_modified_shortcuts);
}

void ReporterLoggingService::SetScannedLocations(
    const std::vector<UwS::TraceLocation>& scanned_locations) {
  base::AutoLock lock(lock_);
  for (UwS::TraceLocation location : scanned_locations)
    reporter_logs_.add_scanned_locations(location);
}

void ReporterLoggingService::LogProcessInformation(
    SandboxType process_type,
    const SystemResourceUsage& usage) {
  ProcessInformation info =
      GetProcessInformationProtoObject(process_type, usage);
  base::AutoLock lock(lock_);
  reporter_logs_.add_process_information()->Swap(&info);
}

bool ReporterLoggingService::AllExpectedRemovalsConfirmed() const {
  // This function should never be called on reporter logging service as no
  // files are removed by the reporter. Return |false| as the default value to
  // indicate error if it ever happens.
  NOTREACHED();
  return false;
}

std::string ReporterLoggingService::RawReportContent() {
  base::AutoLock lock(lock_);
  std::string serialized_report;
  reporter_logs_.SerializeToString(&serialized_report);
  return serialized_report;
}

bool ReporterLoggingService::ReadContentFromFile(
    const base::FilePath& log_file) {
  return true;
}

void ReporterLoggingService::ScheduleFallbackLogsUpload(
    RegistryLogger* registry_logger,
    ResultCode result_code) {}

void ReporterLoggingService::OnReportUploadResult(
    const UploadResultCallback& done_callback,
    RegistryLogger* registry_logger,
    SafeBrowsingReporter::Result result,
    const std::string& serialized_report,
    std::unique_ptr<ChromeFoilResponse> response) {
  registry_logger->WriteReporterLogsUploadResult(result);
  done_callback.Run(result == SafeBrowsingReporter::Result::UPLOAD_SUCCESS);
}

bool ReporterLoggingService::IsReportingNeeded() const {
  base::AutoLock lock(const_cast<base::Lock&>(lock_));
  // We should only upload logs if we have explicitly set an exit code and if
  // there is at least one UwS detected.
  return reporter_logs_.exit_code() != RESULT_CODE_PENDING &&
         reporter_logs_.detected_uws_size() > 0;
}

}  // namespace chrome_cleaner
