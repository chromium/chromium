// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/cleaner_logging_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/win/i18n.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/chrome_utils/chrome_util.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/chrome_cleanup_tool_branding.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "chrome/chrome_cleaner/logging/api_keys.h"
#include "chrome/chrome_cleaner/logging/pending_logs_service.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/logging/utils.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/file_removal_status_updater.h"
#include "chrome/chrome_cleaner/os/rebooter.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/resource_util.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/strings/string_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

namespace {
// TODO(joenotcharles): Refer to the report definition in the "data" section.
constexpr net::NetworkTrafficAnnotationTag kCleanerReportTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chrome_cleanup_report", R"(
          semantics {
            sender: "Chrome Cleanup"
            description:
              "Chrome on Windows is able to detect and remove software that "
              "violates Google's Unwanted Software Policy "
              "(https://www.google.com/about/unwanted-software-policy.html). "
              "When potentially unwanted software is detected in the "
              "background, Chrome offers to remove it. If the user accepts the "
              "cleanup and chooses to \"Report details to Google\", Chrome "
              "will upload details of the unwanted software and its removal, "
              "as well as some details about the system, to help Google track "
              "the spread of unwanted software. "
              "The user can also use the settings page to ask Chrome to search "
              "for unwanted software and remove it. In this case if the user "
              "chooses \"Report details to Google\", the system details will "
              "be uploaded to Google whether or not unwanted software is found."
            trigger:
              "The user either accepted a prompt to remove unwanted software, "
              "or went to \"Clean up computer\" in the settings page and chose "
              "to \"Find harmful software\", and enabled \"Report details to "
              "Google\"."
            data:
              "The user's Chrome version, Windows version, and locale, file "
              "metadata related to the unwanted software that was detected, "
              "automatically installed Chrome extensions, and system settings "
              "commonly used by malicious software as described at "
              "https://www.google.com/chrome/privacy/whitepaper.html#unwantedsoftware. "
              "Contents of files are never reported. No user identifiers are "
              "reported, and common user identifiers found in metadata are "
              "replaced with generic strings, but it is possible some metadata "
              "may contain personally identifiable information. The complete "
              "data specification is at "
              "https://cs.chromium.org/chromium/src/chrome/chrome_cleaner/logging/proto/chrome_cleaner_report.proto."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Chrome Cleanup can be explicitly requested by the user in "
              "\"Clean up computer\" in the \"Reset and cleanup\" section of "
              "settings, under Advanced. To disable this report, turn off "
              "\"Report details to Google\" before choosing \"Find and remove "
              "harmful software\"."
            chrome_policy {
              ChromeCleanupReportingEnabled {
                ChromeCleanupReportingEnabled: false
              }
              ChromeCleanupEnabled {
                ChromeCleanupEnabled: false
              }
            }
          }
          comments:
            "If ChromeCleanupEnabled is set to \"false\", Chrome Cleanup will "
            "never run, so no reports will be uploaded to Google. Otherwise, "
            "ChromeCleanupReportingEnabled can be used to override the "
            "\"Report details to Google\" control: if it is set to \"true\", "
            "reports will always be sent, and if it is set to \"false\", "
            "reports will never be sent."
          )");

// Convert a FileInformation proto object to its corresponding
// internal::FileInformation struct. This function is thread lock safe.
void ProtoObjectToFileInformation(const FileInformation& proto_file_information,
                                  internal::FileInformation* file_information) {
  DCHECK(file_information);
  file_information->path = base::UTF8ToWide(proto_file_information.path());
  file_information->creation_date = proto_file_information.creation_date();
  file_information->last_modified_date =
      proto_file_information.last_modified_date();
  file_information->sha256 = proto_file_information.sha256();
  file_information->size = proto_file_information.size();
  file_information->company_name =
      base::UTF8ToWide(proto_file_information.company_name());
  file_information->company_short_name =
      base::UTF8ToWide(proto_file_information.company_short_name());
  file_information->product_name =
      base::UTF8ToWide(proto_file_information.product_name());
  file_information->product_short_name =
      base::UTF8ToWide(proto_file_information.product_short_name());
  file_information->internal_name =
      base::UTF8ToWide(proto_file_information.internal_name());
  file_information->original_filename =
      base::UTF8ToWide(proto_file_information.original_filename());
  file_information->file_description =
      base::UTF8ToWide(proto_file_information.file_description());
  file_information->file_version =
      base::UTF8ToWide(proto_file_information.file_version());
  file_information->active_file = proto_file_information.active_file();
}

}  // namespace

void AppendFileInformation(const FileInformation& file,
                           MessageBuilder* builder) {
  internal::FileInformation file_information;
  ProtoObjectToFileInformation(file, &file_information);
  builder->Add(FileInformationToString(file_information));
}

void AppendFolderInformation(const FolderInformation& folder,
                             MessageBuilder* builder) {
  if (folder.path().empty())
    return;

  builder->Add(L"path = '", folder.path(), L"'");
  if (!folder.creation_date().empty())
    builder->Add(L", folder_creation_date = '", folder.creation_date(), L"'");
  if (!folder.last_modified_date().empty()) {
    builder->Add(L", folder_last_modified_date = '",
                 folder.last_modified_date(), L"'");
  }
}

void AppendMatchedFile(const MatchedFile& file, MessageBuilder* builder) {
  AppendFileInformation(file.file_information(), builder);
  builder->Add(L", removal_status = ", file.removal_status());
  builder->Add(L", quarantine_status = ", file.quarantine_status());
}

void AppendMatchedRegistryEntry(const MatchedRegistryEntry& registry,
                                MessageBuilder* builder) {
  builder->Add(registry.key_path(), L"\\", registry.value_name(), L" ",
               registry.value_substring());
}

void AppendScheduledTask(const ScheduledTask& scheduled_task,
                         MessageBuilder* builder) {
  MessageBuilder::ScopedIndent scoped_indent(builder);
  builder->AddLine(scheduled_task.description(), L" (", scheduled_task.name(),
                   "):");

  MessageBuilder::ScopedIndent scoped_indent_2(builder);
  builder->AddHeaderLine(L"Actions");
  for (auto action : scheduled_task.actions()) {
    MessageBuilder::ScopedIndent scoped_indent_3(builder);
    builder->Add(L"File information: ");
    AppendFileInformation(action.file_information(), builder);
    builder->NewLine();
    builder->AddFieldValueLine(L"Working directory: ", action.working_dir());
    builder->AddFieldValueLine(L"Arguments: ", action.arguments());
  }
}

ChromeCleanerReport::CleanerStartup GetCleanerStartupFromCommandLine(
    const base::CommandLine* command_line) {
  if (!command_line->HasSwitch(kChromePromptSwitch))
    return ChromeCleanerReport::CLEANER_STARTUP_NOT_PROMPTED;

  std::string chrome_prompt_string =
      command_line->GetSwitchValueASCII(kChromePromptSwitch);
  int chrome_prompt_value = 0;
  if (base::StringToInt(chrome_prompt_string, &chrome_prompt_value) &&
      ChromeCleanerReport_CleanerStartup_IsValid(chrome_prompt_value) &&
      (chrome_prompt_value == ChromeCleanerReport::CLEANER_STARTUP_PROMPTED ||
       chrome_prompt_value ==
           ChromeCleanerReport::CLEANER_STARTUP_SHOWN_FROM_MENU ||
       chrome_prompt_value ==
           ChromeCleanerReport::CLEANER_STARTUP_USER_INITIATED)) {
    return static_cast<ChromeCleanerReport::CleanerStartup>(
        chrome_prompt_value);
  }

  LOG(ERROR) << "Invalid value passed to --" << kChromePromptSwitch << ": '"
             << chrome_prompt_string << "'.";
  return ChromeCleanerReport::CLEANER_STARTUP_UNKNOWN;
}

CleanerLoggingService* CleanerLoggingService::GetInstance() {
  return base::Singleton<CleanerLoggingService>::get();
}

// Please avoid setting any state here, all setup should be done in
// SetupInitialState to allow Terminate to reset this class to its default
// state.
CleanerLoggingService::CleanerLoggingService()
    : uploads_enabled_(false),
      initialized_(false),
      sampler_(DetailedInfoSampler::kDefaultMaxFiles) {}

CleanerLoggingService::~CleanerLoggingService() {
  // If initialize is called, the function |Terminate| must be called by the
  // user of this class before deleting this object.
  DCHECK(!initialized_)
      << "'Terminate' must be called before deleting CleanerLoggingService.";
}

void CleanerLoggingService::Initialize(RegistryLogger* registry_logger) {
  DCHECK(!initialized_) << "CleanerLoggingService already initialized.";
  EnableUploads(false, registry_logger);

  FileRemovalStatusUpdater::GetInstance()->Clear();

  logging::SetLogMessageHandler(
      CleanerLoggingService::LogMessageHandlerFunction);

  Settings* settings = Settings::GetInstance();
  const bool metrics_enabled = settings->metrics_enabled();
  const bool sber_enabled = settings->sber_enabled();
  const std::string cleanup_id = settings->cleanup_id();
  const Engine::Name engine = settings->engine();
  const std::string engine_version = settings->engine_version();

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const ChromeCleanerReport::CleanerStartup cleaner_startup =
      GetCleanerStartupFromCommandLine(command_line);
  const bool chrome_prompt =
      cleaner_startup == ChromeCleanerReport::CLEANER_STARTUP_PROMPTED;

  int channel = 0;
  bool has_chrome_channel = GetChromeChannelFromCommandLine(&channel);

  const bool post_reboot = Rebooter::IsPostReboot();

  std::vector<std::wstring> languages;
  base::win::i18n::GetUserPreferredUILanguageList(&languages);

  std::wstring chrome_version_string;
  bool chrome_version_string_succeeded =
      RetrieveChromeVersionAndInstalledDomain(&chrome_version_string, nullptr);

  base::CPU cpu_info;
  std::string cpu_architecture = base::SysInfo::OperatingSystemArchitecture();

  {
    base::AutoLock lock(raw_log_lines_buffer_lock_);
    raw_log_lines_buffer_.clear();
  }

  {
    base::AutoLock lock(lock_);

    // Ensure that logging report starts in a cleared state.
    chrome_cleaner_report_.Clear();
    matched_files_.clear();
    matched_folders_.clear();

    // Initialize with invalid exit code to identify whether it was set or not.
    chrome_cleaner_report_.set_exit_code(RESULT_CODE_PENDING);
    chrome_cleaner_report_.set_intermediate_log(true);
    chrome_cleaner_report_.set_uma_user(metrics_enabled);
    chrome_cleaner_report_.set_sber_enabled(sber_enabled);
    chrome_cleaner_report_.set_post_reboot(post_reboot);
    // TODO(veranika): this field is deprecated. Stop reporting it.
    chrome_cleaner_report_.set_chrome_prompt(chrome_prompt);
    if (cleaner_startup != ChromeCleanerReport::CLEANER_STARTUP_UNSPECIFIED)
      chrome_cleaner_report_.set_cleaner_startup(cleaner_startup);
    chrome_cleaner_report_.set_cleanup_id(cleanup_id);

    // Set invariant environment / machine data.
    ChromeCleanerReport_EnvironmentData* env_data =
        chrome_cleaner_report_.mutable_environment();
    env_data->set_windows_version(static_cast<int>(base::win::GetVersion()));
    env_data->set_cleaner_version(CHROME_CLEANER_VERSION_UTF8_STRING);
    if (languages.size() > 0)
      env_data->set_default_locale(base::WideToUTF8(languages[0]));
    env_data->set_detailed_system_report(false);
    env_data->set_bitness(IsX64Process() ? 64 : 32);

    if (chrome_version_string_succeeded)
      env_data->set_chrome_version(base::WideToUTF8(chrome_version_string));

    if (has_chrome_channel)
      env_data->set_chrome_channel(channel);

    ChromeCleanerReport_EnvironmentData_Machine* machine =
        env_data->mutable_machine();
    machine->set_cpu_architecture(cpu_architecture);
    machine->set_cpu_vendor(cpu_info.vendor_name());
    machine->set_cpuid(cpu_info.signature());

    env_data->mutable_engine()->set_name(engine);
    if (!engine_version.empty())
      env_data->mutable_engine()->set_version(engine_version);

    initialized_ = true;
  }
}

void CleanerLoggingService::Terminate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(initialized_) << "Logging service is not initialized.";

  size_t num_log_lines = 0;
  {
    // Get all values from the logging report we are interested in before
    // clearing it.
    base::AutoLock lock(raw_log_lines_buffer_lock_);
    num_log_lines = raw_log_lines_buffer_.size();
  }

  if (num_log_lines > 0) {
    LOG(WARNING) << "At least the last " << num_log_lines
                 << " log lines have not been uploaded to Safe Browsing.";
  }

  {
    base::AutoLock lock(raw_log_lines_buffer_lock_);
    raw_log_lines_buffer_.clear();
    initialized_ = false;
  }
}

void CleanerLoggingService::SendLogsToSafeBrowsing(
    const UploadResultCallback& done_callback,
    RegistryLogger* registry_logger) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(initialized_);

  // If reporting is not enabled or required, call |done_callback|
  if (!uploads_enabled() || !IsReportingNeeded())
    return done_callback.Run(false);  // false since no logs were uploaded.

  ChromeCleanerReport chrome_cleaner_report;
  GetCurrentChromeCleanerReport(&chrome_cleaner_report);

  {
    base::AutoLock lock(lock_);
    // No need to repeat the log lines in subsequent uploads.
    chrome_cleaner_report_.clear_raw_log_line();
    chrome_cleaner_report_.mutable_environment()->set_detailed_system_report(
        false);
  }

  // TODO(csharp): Move this to the main controller.
  // Register a task to try again if we ever fail half way through uploading
  // |chrome_cleaner_report|. Will be cleared upon success in
  // |OnReportUploadResult|.
  ClearTempLogFile(registry_logger);
  PendingLogsService::ScheduleLogsUploadTask(PRODUCT_SHORTNAME_STRING,
                                             chrome_cleaner_report,
                                             &temp_log_file_, registry_logger);

  std::string serialized_report;
  if (!chrome_cleaner_report.SerializeToString(&serialized_report)) {
    LOG(WARNING) << "Failed to serialize report";
    return done_callback.Run(false);  // false since no logs were uploaded.
  }

  SafeBrowsingReporter::UploadReport(
      base::BindRepeating(&CleanerLoggingService::OnReportUploadResult,
                          base::Unretained(this), done_callback,
                          registry_logger),
      kSafeBrowsingCleanerUrl, serialized_report,
      kCleanerReportTrafficAnnotation);
}

void CleanerLoggingService::CancelWaitForShutdown() {
  SafeBrowsingReporter::CancelWaitForShutdown();
}

void CleanerLoggingService::EnableUploads(bool enable,
                                          RegistryLogger* registry_logger) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (enable == uploads_enabled_)
    return;
  uploads_enabled_ = enable;

  // Make sure not to keep any scheduled logs upload if the user opts-out of
  // logs upload. TODO(csharp): maybe we should also clear all other pending
  // logs, not just ours.
  if (!enable && registry_logger)
    ClearTempLogFile(registry_logger);
}

bool CleanerLoggingService::uploads_enabled() const {
  return uploads_enabled_;
}

void CleanerLoggingService::SetDetailedSystemReport(
    bool detailed_system_report) {
  base::AutoLock lock(lock_);
  chrome_cleaner_report_.mutable_environment()->set_detailed_system_report(
      detailed_system_report);
}

bool CleanerLoggingService::detailed_system_report_enabled() const {
  return chrome_cleaner_report_.environment().detailed_system_report();
}

void CleanerLoggingService::AddFoundUwS(const std::string& found_uws_name) {
  base::AutoLock lock(lock_);
  chrome_cleaner_report_.add_found_uws(found_uws_name);
}

void CleanerLoggingService::AddDetectedUwS(const PUPData::PUP* found_uws,
                                           UwSDetectedFlags flags) {
  DCHECK(found_uws);
  UwS detected_uws =
      PUPToUwS(found_uws, flags, /*cleaning_files=*/true, &sampler_);
  AddDetectedUwS(detected_uws);
}

void CleanerLoggingService::AddDetectedUwS(const UwS& uws) {
  base::AutoLock lock(lock_);
  UwS* added_uws = chrome_cleaner_report_.add_detected_uws();
  *added_uws = uws;
  UpdateMatchedFilesAndFoldersMaps(added_uws);
}

void CleanerLoggingService::SetExitCode(ResultCode exit_code) {
  ResultCode previous_exit_code = RESULT_CODE_INVALID;
  {
    base::AutoLock lock(lock_);
    previous_exit_code =
        static_cast<ResultCode>(chrome_cleaner_report_.exit_code());
    chrome_cleaner_report_.set_exit_code(exit_code);
    // Once an exit code has been provided, this is not an intermediate log
    // anymore.
    chrome_cleaner_report_.set_intermediate_log(false);
  }
  // The DCHECK can't be under |lock_|. The only valid reason to overwrite a non
  // pending exit code, is when we failed to read pending upload log files.
  DCHECK(previous_exit_code == RESULT_CODE_PENDING ||
         exit_code == RESULT_CODE_FAILED_TO_READ_UPLOAD_LOGS_FILE);
}

void CleanerLoggingService::AddLoadedModule(
    const std::wstring& name,
    ModuleHost module_host,
    const internal::FileInformation& file_information) {
  FileInformation reported_file_information;
  FileInformationToProtoObject(file_information, &reported_file_information);

  base::AutoLock lock(lock_);
  ChromeCleanerReport::SystemReport::LoadedModule* loaded_module =
      chrome_cleaner_report_.mutable_system_report()->add_loaded_modules();
  loaded_module->set_name(base::WideToUTF8(name));
  loaded_module->set_host(module_host);
  *loaded_module->mutable_file_information() = reported_file_information;
}

void CleanerLoggingService::AddService(
    const std::wstring& display_name,
    const std::wstring& service_name,
    const internal::FileInformation& file_information) {
  FileInformation reported_file_information;
  FileInformationToProtoObject(file_information, &reported_file_information);

  base::AutoLock lock(lock_);
  ChromeCleanerReport::SystemReport::Service* service =
      chrome_cleaner_report_.mutable_system_report()->add_services();
  service->set_display_name(base::WideToUTF8(display_name));
  service->set_service_name(base::WideToUTF8(service_name));
  *service->mutable_file_information() = reported_file_information;
}

void CleanerLoggingService::AddInstalledProgram(
    const base::FilePath& folder_path) {
  FolderInformation folder_information;
  if (!RetrieveFolderInformation(folder_path, &folder_information))
    return;

  base::AutoLock lock(lock_);
  ChromeCleanerReport::SystemReport::InstalledProgram* installed_program =
      chrome_cleaner_report_.mutable_system_report()->add_installed_programs();
  *installed_program->mutable_folder_information() = folder_information;
}

void CleanerLoggingService::AddProcess(
    const std::wstring& name,
    const internal::FileInformation& file_information) {
  FileInformation reported_file_information;
  FileInformationToProtoObject(file_information, &reported_file_information);

  base::AutoLock lock(lock_);
  ChromeCleanerReport::SystemReport::Process* process =
      chrome_cleaner_report_.mutable_system_report()->add_processes();
  process->set_name(base::WideToUTF8(name));
  *process->mutable_file_information() = reported_file_information;
}

void CleanerLoggingService::AddRegistryValue(
    const internal::RegistryValue& registry_value,
    const std::vector<internal::FileInformation>& file_informations) {
  RegistryValue new_registry_value;
  new_registry_value.set_key_path(base::WideToUTF8(registry_value.key_path));
  new_registry_value.set_value_name(
      base::WideToUTF8(registry_value.value_name));
  new_registry_value.set_data(base::WideToUTF8(registry_value.data));

  for (const auto& file_information : file_informations) {
    FileInformation* reported_file_information =
        new_registry_value.add_file_informations();
    FileInformationToProtoObject(file_information, reported_file_information);
  }

  base::AutoLock lock(lock_);
  *chrome_cleaner_report_.mutable_system_report()->add_registry_values() =
      new_registry_value;
}

void CleanerLoggingService::AddLayeredServiceProvider(
    const std::vector<std::wstring>& guids,
    const internal::FileInformation& file_information) {
  ChromeCleanerReport_SystemReport_LayeredServiceProvider
      layered_service_provider;
  FileInformation* reported_file_information =
      layered_service_provider.mutable_file_information();
  FileInformationToProtoObject(file_information, reported_file_information);

  for (const auto& guid : guids)
    layered_service_provider.add_guids(base::WideToUTF8(guid));

  base::AutoLock lock(lock_);
  *chrome_cleaner_report_.mutable_system_report()
       ->add_layered_service_providers() = layered_service_provider;
}

void CleanerLoggingService::SetWinInetProxySettings(
    const std::wstring& config,
    const std::wstring& bypass,
    const std::wstring& auto_config_url,
    bool autodetect) {
  base::AutoLock lock(lock_);
  ChromeCleanerReport_SystemReport_SystemProxySettings*
      win_inet_proxy_settings = chrome_cleaner_report_.mutable_system_report()
                                    ->mutable_win_inet_proxy_settings();
  win_inet_proxy_settings->set_config(base::WideToUTF8(config));
  win_inet_proxy_settings->set_bypass(base::WideToUTF8(bypass));
  win_inet_proxy_settings->set_auto_config_url(
      base::WideToUTF8(auto_config_url));
  win_inet_proxy_settings->set_autodetect(autodetect);
}

void CleanerLoggingService::SetWinHttpProxySettings(
    const std::wstring& config,
    const std::wstring& bypass) {
  base::AutoLock lock(lock_);
  ChromeCleanerReport_SystemReport_SystemProxySettings*
      win_http_proxy_settings = chrome_cleaner_report_.mutable_system_report()
                                    ->mutable_win_http_proxy_settings();
  win_http_proxy_settings->set_config(base::WideToUTF8(config));
  win_http_proxy_settings->set_bypass(base::WideToUTF8(bypass));
}

void CleanerLoggingService::AddInstalledExtension(
    const std::wstring& extension_id,
    ExtensionInstallMethod install_method,
    const std::vector<internal::FileInformation>& extension_files) {
  base::AutoLock lock(lock_);
  ChromeCleanerReport_SystemReport_InstalledExtension* installed_extension =
      chrome_cleaner_report_.mutable_system_report()
          ->add_installed_extensions();
  installed_extension->set_extension_id(base::WideToUTF8(extension_id));
  installed_extension->set_install_method(install_method);
  for (const auto& file : extension_files) {
    FileInformation proto_file_information;
    FileInformationToProtoObject(file, &proto_file_information);
    *installed_extension->add_extension_files() = proto_file_information;
  }
}

void CleanerLoggingService::AddScheduledTask(
    const std::wstring& name,
    const std::wstring& description,
    const std::vector<internal::FileInformation>& actions) {
  ScheduledTask scheduled_task;
  scheduled_task.set_name(base::WideToUTF8(name));
  scheduled_task.set_description(base::WideToUTF8(description));

  for (const auto& action : actions) {
    FileInformation* reported_action =
        scheduled_task.add_actions()->mutable_file_information();
    FileInformationToProtoObject(action, reported_action);
  }

  base::AutoLock lock(lock_);
  *chrome_cleaner_report_.mutable_system_report()->add_scheduled_tasks() =
      scheduled_task;
}

void CleanerLoggingService::AddShortcutData(
    const std::wstring& lnk_path,
    const std::wstring& executable_path,
    const std::string& executable_hash,
    const std::vector<std::wstring>& command_line_arguments) {
  base::AutoLock lock(lock_);
  ChromeCleanerReport_SystemReport_ShortcutData* shortcut_data =
      chrome_cleaner_report_.mutable_system_report()->add_shortcut_data();
  shortcut_data->set_lnk_path(base::WideToUTF8(lnk_path));
  shortcut_data->set_executable_path(base::WideToUTF8(executable_path));
  shortcut_data->set_executable_hash(executable_hash);
  for (const auto& argument : command_line_arguments) {
    shortcut_data->add_command_line_arguments(base::WideToUTF8(argument));
  }
}

void CleanerLoggingService::SetFoundModifiedChromeShortcuts(
    bool /*found_modified_shortcuts*/) {}

void CleanerLoggingService::SetScannedLocations(
    const std::vector<UwS::TraceLocation>& /*scanned_locations*/) {}

void CleanerLoggingService::LogProcessInformation(
    SandboxType process_type,
    const SystemResourceUsage& usage) {
  ProcessInformation info =
      GetProcessInformationProtoObject(process_type, usage);
  base::AutoLock lock(lock_);
  chrome_cleaner_report_.add_process_information()->Swap(&info);
}

bool CleanerLoggingService::AllExpectedRemovalsConfirmed() const {
  FileRemovalStatusUpdater* status_updater =
      FileRemovalStatusUpdater::GetInstance();

  base::AutoLock lock(lock_);
  for (const UwS& uws : chrome_cleaner_report_.detected_uws()) {
    if (uws.state() != UwS::REMOVABLE)
      continue;
    for (const MatchedFile& file : uws.files()) {
      std::wstring sanitized_path =
          base::UTF8ToWide(file.file_information().path());
      RemovalStatus removal_status =
          status_updater->GetRemovalStatusOfSanitizedPath(sanitized_path);

      // If the removal status was never set in FileRemovalStatusUpdater, fall
      // back to the status that was originally set when the MatchedFile object
      // was created.
      if (removal_status == REMOVAL_STATUS_UNSPECIFIED)
        removal_status = file.removal_status();

      // Non-active files are collected in the report for later auditing, but
      // not expected to be removed.
      if (removal_status == REMOVAL_STATUS_MATCHED_ONLY &&
          !file.file_information().active_file()) {
        continue;
      }

      if (removal_status != REMOVAL_STATUS_REMOVED &&
          removal_status != REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL &&
          removal_status != REMOVAL_STATUS_NOT_FOUND &&
          removal_status != REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK) {
        return false;
      }
    }
  }
  return true;
}

std::string CleanerLoggingService::RawReportContent() {
  ChromeCleanerReport chrome_cleaner_report;
  GetCurrentChromeCleanerReport(&chrome_cleaner_report);

  std::string chrome_cleaner_report_string;
  chrome_cleaner_report.SerializeToString(&chrome_cleaner_report_string);
  return chrome_cleaner_report_string;
}

bool CleanerLoggingService::ReadContentFromFile(
    const base::FilePath& log_file) {
  std::string proto_string;
  if (!base::ReadFileToString(log_file, &proto_string)) {
    LOG(ERROR) << "Can't read content of '" << SanitizePath(log_file) << "'.";
    return false;
  } else if (proto_string.empty()) {
    LOG(ERROR) << "Empty log file: " << SanitizePath(log_file);
    return false;
  }
  bool succeeded = false;
  {
    base::AutoLock lock(lock_);
    succeeded = chrome_cleaner_report_.ParseFromString(proto_string);
    if (succeeded) {
      matched_files_.clear();
      matched_folders_.clear();
      for (UwS& detected_uws : *chrome_cleaner_report_.mutable_detected_uws())
        UpdateMatchedFilesAndFoldersMaps(&detected_uws);
    }
  }
  if (!succeeded) {
    LOG(ERROR) << "Read invalid protobuf from '" << SanitizePath(log_file)
               << "'.";
    return false;
  }
  return true;
}

void CleanerLoggingService::ScheduleFallbackLogsUpload(
    RegistryLogger* registry_logger,
    ResultCode result_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(initialized_);
  // Even if we don't upload logs, we can still let Chrome UMA know we got to
  // this stage.
  DCHECK(registry_logger);
  registry_logger->WriteExitCode(result_code);

  if (!uploads_enabled())
    return;

  ChromeCleanerReport chrome_cleaner_report;
  GetCurrentChromeCleanerReport(&chrome_cleaner_report);
  chrome_cleaner_report.set_exit_code(result_code);

  ClearTempLogFile(registry_logger);
  PendingLogsService::ScheduleLogsUploadTask(PRODUCT_SHORTNAME_STRING,
                                             chrome_cleaner_report,
                                             &temp_log_file_, registry_logger);
}

void CleanerLoggingService::OnReportUploadResult(
    const UploadResultCallback& done_callback,
    RegistryLogger* registry_logger,
    SafeBrowsingReporter::Result result,
    const std::string& serialized_report,
    std::unique_ptr<ChromeFoilResponse> response) {
  DCHECK(registry_logger);
  if (result == SafeBrowsingReporter::Result::UPLOAD_SUCCESS)
    ClearTempLogFile(registry_logger);
  done_callback.Run(result == SafeBrowsingReporter::Result::UPLOAD_SUCCESS);
}

bool CleanerLoggingService::IsReportingNeeded() const {
  Settings* settings = Settings::GetInstance();
  if (settings->execution_mode() != ExecutionMode::kScanning &&
      settings->execution_mode() != ExecutionMode::kCleanup) {
    NOTREACHED();
    return false;
  }

  // Raw log lines are collected in vector raw_log_lines_buffer_ and moved to
  // proto field raw_log_line whenever the proto is generated. Logs should be
  // uploaded if either is non-empty.

  {
    base::AutoLock lock(raw_log_lines_buffer_lock_);
    if (!raw_log_lines_buffer_.empty())
      return true;
  }

  {
    base::AutoLock lock(lock_);
    return chrome_cleaner_report_.exit_code() != RESULT_CODE_PENDING ||
           chrome_cleaner_report_.raw_log_line_size() > 0 ||
           chrome_cleaner_report_.found_uws_size() > 0;
  }
}

void CleanerLoggingService::ClearTempLogFile(RegistryLogger* registry_logger) {
  if (!temp_log_file_.empty()) {
    PendingLogsService::ClearPendingLogFile(PRODUCT_SHORTNAME_STRING,
                                            temp_log_file_, registry_logger);
    temp_log_file_.clear();
  }
}

// static.
bool CleanerLoggingService::LogMessageHandlerFunction(int severity,
                                                      const char* file,
                                                      int line,
                                                      size_t message_start,
                                                      const std::string& str) {
  CleanerLoggingService* logging_service = CleanerLoggingService::GetInstance();
  std::string utf8_str;
  if (base::IsStringUTF8(str))
    utf8_str = str;
  else
    utf8_str = RemoveInvalidUTF8Chars(str);

  base::AutoLock lock(logging_service->raw_log_lines_buffer_lock_);
  logging_service->raw_log_lines_buffer_.push_back(utf8_str);

  // Returning false, pretending the event wasn't handled here, let the other
  // handlers receive it.
  return false;
}

void CleanerLoggingService::GetCurrentChromeCleanerReport(
    ChromeCleanerReport* chrome_cleaner_report) {
  DCHECK(chrome_cleaner_report);

  UpdateFileRemovalStatuses();

  std::vector<std::string> raw_log_lines_to_send;
  {
    base::AutoLock lock(raw_log_lines_buffer_lock_);
    raw_log_lines_to_send.reserve(raw_log_lines_buffer_.size());
    raw_log_lines_to_send.insert(raw_log_lines_to_send.begin(),
                                 raw_log_lines_buffer_.begin(),
                                 raw_log_lines_buffer_.end());
    raw_log_lines_buffer_.clear();
  }

  {
    base::AutoLock lock(lock_);
    for (const std::string& line : raw_log_lines_to_send)
      chrome_cleaner_report_.add_raw_log_line(line);
    chrome_cleaner_report->CopyFrom(chrome_cleaner_report_);
  }
}

void CleanerLoggingService::UpdateMatchedFilesAndFoldersMaps(UwS* added_uws) {
  for (MatchedFile& file : *added_uws->mutable_files())
    matched_files_[file.file_information().path()].push_back(&file);
  for (MatchedFolder& folder : *added_uws->mutable_folders())
    matched_folders_[folder.folder_information().path()].push_back(&folder);
}

void CleanerLoggingService::UpdateFileRemovalStatuses() {
  for (const auto& path_and_status :
       FileRemovalStatusUpdater::GetInstance()->GetAllRemovalStatuses()) {
    std::string sanitized_path = base::WideToUTF8(path_and_status.first);
    FileRemovalStatusUpdater::FileRemovalStatus status = path_and_status.second;
    DCHECK(status.removal_status != REMOVAL_STATUS_UNSPECIFIED);

    bool known_matched_file = true;
    {
      base::AutoLock lock(lock_);

      auto file_it = matched_files_.find(sanitized_path);
      auto folder_it = matched_folders_.find(sanitized_path);

      if (file_it != matched_files_.end()) {
        for (MatchedFile* matched_file : file_it->second) {
          matched_file->set_removal_status(status.removal_status);
          matched_file->set_quarantine_status(status.quarantine_status);
        }
      } else if (folder_it != matched_folders_.end()) {
        for (MatchedFolder* matched_folder : folder_it->second) {
          matched_folder->set_removal_status(status.removal_status);
          // We don't quarantine folders. So the quarantine status should be
          // |QUARANTINE_STATUS_UNSPECIFIED| and we don't need to record it.
          DCHECK(status.quarantine_status == QUARANTINE_STATUS_UNSPECIFIED);
        }
      } else {
        known_matched_file = false;
      }
    }

    // Files that were deleted should be matched with an UwS by calling
    // AddDetectedUwS before logging. But there are some circumstances where
    // AddDetectedUwS is not called, such as when there are no existing files
    // for that UwS at the moment it is logged, so it should not be marked
    // removable. These should match up to circumstances where no files are
    // deleted, but just in case, log any file deletions that aren't matched to
    // an UwS for investigation.
    if (!known_matched_file) {
      FileInformation file_information;
      bool got_file_information = GetFileInformationProtoObject(
          status.path, /*detailed_information=*/true, &file_information);

      base::AutoLock lock(lock_);
      // Since the item might have been deleted at this point, just assume it
      // was a file.
      MatchedFile* file = nullptr;
      auto* unknown_uws = chrome_cleaner_report_.mutable_unknown_uws();
      for (int i = 0; i < unknown_uws->files_size(); i++) {
        if (unknown_uws->files(i).file_information().path() == sanitized_path) {
          file = unknown_uws->mutable_files(i);
          break;
        }
      }

      if (file == nullptr)
        file = chrome_cleaner_report_.mutable_unknown_uws()->add_files();

      if (got_file_information) {
        *file->mutable_file_information() = file_information;
      } else {
        // If we can't get the detailed file information, still record at least
        // the path.
        file->mutable_file_information()->set_path(sanitized_path);
      }
      file->set_removal_status(status.removal_status);
      file->set_quarantine_status(status.quarantine_status);
    }
  }
}

}  // namespace chrome_cleaner
