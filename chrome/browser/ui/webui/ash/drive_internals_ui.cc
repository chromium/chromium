// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/drive_internals_ui.h"

#include <stddef.h>
#include <stdint.h>

#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/services/file_util/public/cpp/zip_file_creator.h"
#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/public/common/download_item.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/event_logger.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/common/time_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "net/base/filename_util.h"

namespace ash {
namespace {

using base::FileEnumerator;
using base::FilePath;
using base::Value;
using content::BrowserThread;
using drive::DriveIntegrationService;
using drive::prefs::kDriveFsBulkPinningEnabled;
using drivefs::pinning::PinManager;

constexpr char kKey[] = "key";
constexpr char kValue[] = "value";
constexpr char kClass[] = "class";

constexpr const char* const kLogLevelName[] = {"info", "warning", "error"};

size_t SeverityToLogLevelNameIndex(logging::LogSeverity severity) {
  if (severity <= logging::LOGGING_INFO) {
    return 0;
  }
  if (severity == logging::LOGGING_WARNING) {
    return 1;
  }
  return 2;
}

size_t LogMarkToLogLevelNameIndex(char mark) {
  switch (mark) {
    case 'I':
    case 'V':
      return 0;
    case 'W':
      return 1;
    default:
      return 2;
  }
}

template <typename T>
std::string ToString(const T x) {
  return (std::ostringstream() << drivefs::pinning::NiceNum << x).str();
}

template <typename T>
std::string ToPercent(const T num, const T total) {
  if (num >= 0 && total > 0) {
    return (std::ostringstream() << (100 * num / total) << "%").str();
  }

  return "🤔";
}

// Gets metadata of all files and directories in |root_path|
// recursively. Stores the result as a list of dictionaries like:
//
// [{ path: 'GCache/v1/tmp/<local_id>',
//    size: 12345,
//    is_directory: false,
//    last_modified: '2005-08-09T09:57:00-08:00',
//  },...]
//
// The list is sorted by the path.
std::pair<Value::List, Value::Dict> GetGCacheContents(
    const FilePath& root_path) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Use this map to sort the result list by the path.
  std::map<FilePath, Value::Dict> files;

  const int options = (FileEnumerator::FILES | FileEnumerator::DIRECTORIES |
                       FileEnumerator::SHOW_SYM_LINKS);
  FileEnumerator enumerator(root_path, true /* recursive */, options);

  int64_t total_size = 0;
  for (FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    FileEnumerator::FileInfo info = enumerator.GetInfo();
    int64_t size = info.GetSize();
    const bool is_directory = info.IsDirectory();
    const bool is_symbolic_link = base::IsLink(info.GetName());
    const base::Time last_modified = info.GetLastModifiedTime();

    Value::Dict entry;
    entry.Set("path", current.value());
    // Use double instead of integer for large files.
    entry.Set("size", static_cast<double>(size));
    entry.Set("is_directory", is_directory);
    entry.Set("is_symbolic_link", is_symbolic_link);
    entry.Set("last_modified",
              google_apis::util::FormatTimeAsStringLocaltime(last_modified));
    // Print lower 9 bits in octal format.
    entry.Set("permission",
              base::StringPrintf("%03o", info.stat().st_mode & 0x1ff));
    files[current] = std::move(entry);

    total_size += size;
  }

  std::pair<Value::List, Value::Dict> result;
  // Convert |files| into response.
  for (auto& it : files) {
    result.first.Append(std::move(it.second));
  }
  result.second.Set("total_size", static_cast<double>(total_size));
  return result;
}

// Appends {'key': key, 'value': value, 'class': clazz} dictionary to the
// |list|.
void AppendKeyValue(Value::List& list,
                    std::string key,
                    std::string value,
                    std::string clazz = std::string()) {
  Value::Dict dict;
  dict.Set(kKey, std::move(key));
  dict.Set(kValue, std::move(value));
  if (!clazz.empty()) {
    dict.Set(kClass, std::move(clazz));
  }
  list.Append(std::move(dict));
}

ino_t GetInodeValue(const FilePath& path) {
  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0) {
    return 0;
  }
  return file_stats.st_ino;
}

std::pair<ino_t, Value::List> GetServiceLogContents(const FilePath& log_path,
                                                    ino_t inode,
                                                    int from_line_number) {
  Value::List result;

  std::ifstream log(log_path.value());
  if (log.good()) {
    ino_t new_inode = GetInodeValue(log_path);
    if (new_inode != inode) {
      // Apparently log was recreated. Re-read the log.
      from_line_number = 0;
      inode = new_inode;
    }

    base::Time time;
    constexpr char kTimestampPattern[] = R"(????-??-??T??:??:??.???Z? )";
    const size_t pattern_length = strlen(kTimestampPattern);

    std::string line;
    int line_number = 0;
    while (log.good()) {
      std::getline(log, line);
      if (line.empty() || ++line_number <= from_line_number) {
        continue;
      }

      base::StringPiece log_line = line;
      size_t severity_index = 0;
      if (base::MatchPattern(log_line.substr(0, pattern_length),
                             kTimestampPattern) &&
          google_apis::util::GetTimeFromString(
              log_line.substr(0, pattern_length - 2), &time)) {
        severity_index = LogMarkToLogLevelNameIndex(line[pattern_length - 2]);
        line = line.substr(pattern_length);
      }
      const char* const severity = kLogLevelName[severity_index];

      AppendKeyValue(result,
                     google_apis::util::FormatTimeAsStringLocaltime(time),
                     base::StrCat({"[", severity, "] ", line}),
                     base::StrCat({"log-", severity}));
    }
  }

  return {inode, std::move(result)};
}

bool GetDeveloperMode() {
  std::string output;
  if (!base::GetAppOutput({"/usr/bin/crossystem", "cros_debug"}, &output)) {
    return false;
  }
  return output == "1";
}

class DriveInternalsWebUIHandler;

void ZipLogs(Profile* profile,
             base::WeakPtr<DriveInternalsWebUIHandler> drive_internals);

// Class to handle messages from chrome://drive-internals.
class DriveInternalsWebUIHandler : public content::WebUIMessageHandler,
                                   DriveIntegrationService::Observer {
 public:
  ~DriveInternalsWebUIHandler() override {
    if (DriveIntegrationService* const service = GetIntegrationService()) {
      service->RemoveObserver(this);
    }
  }

  DriveInternalsWebUIHandler() = default;
  DriveInternalsWebUIHandler(const DriveInternalsWebUIHandler&) = delete;
  DriveInternalsWebUIHandler& operator=(const DriveInternalsWebUIHandler&) =
      delete;

  void DownloadLogsZip(const FilePath& path) {
    web_ui()->GetWebContents()->GetController().LoadURL(
        net::FilePathToFileURL(path), {}, {}, {});
  }

  void OnZipDone() { MaybeCallJavascript("onZipDone", Value()); }

 private:
  void MaybeCallJavascript(const std::string& function,
                           Value data1,
                           Value data2 = {}) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (IsJavascriptAllowed()) {
      CallJavascriptFunction(function, std::move(data1), std::move(data2));
    }
  }

  // Hide or show a section of the page.
  void SetSectionEnabled(const std::string& section, bool enable) {
    MaybeCallJavascript("setSectionEnabled", Value(section), Value(enable));
  }

  // WebUIMessageHandler override.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "pageLoaded",
        base::BindRepeating(&DriveInternalsWebUIHandler::OnPageLoaded,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "periodicUpdate",
        base::BindRepeating(&DriveInternalsWebUIHandler::OnPeriodicUpdate,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "setVerboseLoggingEnabled",
        base::BindRepeating(
            &DriveInternalsWebUIHandler::SetVerboseLoggingEnabled,
            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "setMirroringEnabled",
        base::BindRepeating(&DriveInternalsWebUIHandler::SetMirroringEnabled,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "addSyncPath",
        base::BindRepeating(&DriveInternalsWebUIHandler::ToggleSyncPath,
                            weak_ptr_factory_.GetWeakPtr(),
                            drivefs::mojom::MirrorPathStatus::kStart));
    web_ui()->RegisterMessageCallback(
        "removeSyncPath",
        base::BindRepeating(&DriveInternalsWebUIHandler::ToggleSyncPath,
                            weak_ptr_factory_.GetWeakPtr(),
                            drivefs::mojom::MirrorPathStatus::kStop));
    web_ui()->RegisterMessageCallback(
        "enableTracing",
        base::BindRepeating(&DriveInternalsWebUIHandler::SetTracingEnabled,
                            weak_ptr_factory_.GetWeakPtr(), true));
    web_ui()->RegisterMessageCallback(
        "disableTracing",
        base::BindRepeating(&DriveInternalsWebUIHandler::SetTracingEnabled,
                            weak_ptr_factory_.GetWeakPtr(), false));
    web_ui()->RegisterMessageCallback(
        "restartDrive",
        base::BindRepeating(&DriveInternalsWebUIHandler::RestartDrive,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "resetDriveFileSystem",
        base::BindRepeating(&DriveInternalsWebUIHandler::ResetDriveFileSystem,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "zipLogs",
        base::BindRepeating(&DriveInternalsWebUIHandler::ZipDriveFsLogs,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void RegisterDeveloperMessages() {
    CHECK(developer_mode_);
    web_ui()->RegisterMessageCallback(
        "setStartupArguments",
        base::BindRepeating(&DriveInternalsWebUIHandler::SetStartupArguments,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "enableNetworking",
        base::BindRepeating(&DriveInternalsWebUIHandler::SetNetworkingEnabled,
                            weak_ptr_factory_.GetWeakPtr(), true));
    web_ui()->RegisterMessageCallback(
        "disableNetworking",
        base::BindRepeating(&DriveInternalsWebUIHandler::SetNetworkingEnabled,
                            weak_ptr_factory_.GetWeakPtr(), false));
    web_ui()->RegisterMessageCallback(
        "enableForcePauseSyncing",
        base::BindRepeating(&DriveInternalsWebUIHandler::ForcePauseSyncing,
                            weak_ptr_factory_.GetWeakPtr(), true));
    web_ui()->RegisterMessageCallback(
        "disableForcePauseSyncing",
        base::BindRepeating(&DriveInternalsWebUIHandler::ForcePauseSyncing,
                            weak_ptr_factory_.GetWeakPtr(), false));
    web_ui()->RegisterMessageCallback(
        "dumpAccountSettings",
        base::BindRepeating(&DriveInternalsWebUIHandler::DumpAccountSettings,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "loadAccountSettings",
        base::BindRepeating(&DriveInternalsWebUIHandler::LoadAccountSettings,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "updateBulkPinningMaxQueueSize",
        base::BindRepeating(
            &DriveInternalsWebUIHandler::UpdateBulkPinningMaxQueueSize,
            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "setBulkPinningEnabled",
        base::BindRepeating(&DriveInternalsWebUIHandler::SetBulkPinningEnabled,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // Called when the page is first loaded.
  void OnPageLoaded(const Value::List& args) {
    AllowJavascript();

    DriveIntegrationService* const service = GetIntegrationService();
    if (!service) {
      return;
    }

    UpdateDriveRelatedPreferencesSection();
    UpdateGCacheContentsSection();
    UpdatePathConfigurationsSection();
    UpdateConnectionStatusSection();
    UpdateAboutResourceSection();
    UpdateDeltaUpdateStatusSection();
    UpdateCacheContentsSection();
    UpdateInFlightOperationsSection();
    UpdateDriveDebugSection();
    UpdateMirrorSyncSection();

    // When the drive-internals page is reloaded by the reload key, the page
    // content is recreated, but this WebUI object is not (instead, OnPageLoaded
    // is called again). In that case, we have to forget the last sent ID here,
    // and resent whole the logs to the page.
    last_sent_event_id_ = -1;
    UpdateEventLogSection();
    last_sent_line_number_ = 0;
    service_log_file_inode_ = 0;
    UpdateServiceLogSection();
  }

  // Called when the page requests periodic update.
  void OnPeriodicUpdate(const Value::List& args) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    DriveIntegrationService* const service = GetIntegrationService();
    if (!service) {
      return;
    }

    UpdateEventLogSection();
    UpdateServiceLogSection();

    UpdateInFlightOperationsSection();
  }

  //
  // Updates respective sections.
  //
  void UpdateConnectionStatusSection() {
    SetSectionEnabled("connection-status-section", true);

    std::string status;
    switch (drive::util::GetDriveConnectionStatus(profile())) {
      case drive::util::DRIVE_DISCONNECTED_NOSERVICE:
        status = "no service";
        break;
      case drive::util::DRIVE_DISCONNECTED_NONETWORK:
        status = "no network";
        break;
      case drive::util::DRIVE_DISCONNECTED_NOTREADY:
        status = "not ready";
        break;
      case drive::util::DRIVE_CONNECTED_METERED:
        status = "metered";
        break;
      case drive::util::DRIVE_CONNECTED:
        status = "connected";
        break;
    }

    Value::Dict connection_status;
    connection_status.Set("status", std::move(status));
    drive::DriveNotificationManager* const manager =
        drive::DriveNotificationManagerFactory::FindForBrowserContext(
            profile());
    connection_status.Set(
        "push-notification-enabled",
        manager ? manager->push_notification_enabled() : false);

    MaybeCallJavascript("updateConnectionStatus",
                        Value(std::move(connection_status)));
  }

  void UpdateAboutResourceSection() {
    // TODO(crbug.com/896123): Maybe worth implementing.
    SetSectionEnabled("account-information-section", false);
  }

  void UpdateDeltaUpdateStatusSection() {
    // TODO(crbug.com/896123): Maybe worth implementing.
    SetSectionEnabled("delta-update-status-section", false);
  }

  void UpdateInFlightOperationsSection() {
    // TODO(crbug.com/896123): Maybe worth implementing.
    SetSectionEnabled("in-flight-operations-section", false);
  }

  void UpdatePathConfigurationsSection() {
    SetSectionEnabled("path-configurations-section", true);

    Value::List paths;
    AppendKeyValue(paths, "Downloads",
                   file_manager::util::GetDownloadsFolderForProfile(profile())
                       .AsUTF8Unsafe());
    if (DriveIntegrationService* const service = GetIntegrationService()) {
      AppendKeyValue(paths, "Drive",
                     service->GetMountPointPath().AsUTF8Unsafe());
    }

    const char* const kPathPreferences[] = {
        prefs::kSelectFileLastDirectory,
        prefs::kSaveFileDefaultDirectory,
        prefs::kDownloadDefaultDirectory,
    };

    for (const char* key : kPathPreferences) {
      AppendKeyValue(paths, key, GetPrefs()->GetFilePath(key).AsUTF8Unsafe());
    }

    MaybeCallJavascript("updatePathConfigurations", Value(std::move(paths)));
  }

  void UpdateDriveDebugSection() {
    SetSectionEnabled("drive-debug", true);

    bool verbose_logging_enabled =
        GetPrefs()->GetBoolean(drive::prefs::kDriveFsEnableVerboseLogging);
    MaybeCallJavascript("updateVerboseLogging", Value(verbose_logging_enabled));

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(GetDeveloperMode),
        base::BindOnce(&DriveInternalsWebUIHandler::OnGetDeveloperMode,
                       weak_ptr_factory_.GetWeakPtr()));

    // Propagate the amount of local free space in bytes.
    FilePath home_path;
    if (base::PathService::Get(base::DIR_HOME, &home_path)) {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, home_path),
          base::BindOnce(&DriveInternalsWebUIHandler::OnGetFreeDiskSpace,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      LOG(ERROR) << "Home directory not found";
    }
  }

  void UpdateMirrorSyncSection() {
    if (!features::IsDriveFsMirroringEnabled()) {
      SetSectionEnabled("mirror-sync-section", false);
      return;
    }

    SetSectionEnabled("mirror-sync-section", true);

    bool mirroring_enabled =
        GetPrefs()->GetBoolean(drive::prefs::kDriveFsEnableMirrorSync);
    MaybeCallJavascript("updateMirroring", Value(mirroring_enabled));
    SetSectionEnabled("mirror-sync-paths", mirroring_enabled);
    SetSectionEnabled("mirror-path-form", mirroring_enabled);
    if (!mirroring_enabled) {
      return;
    }

    DriveIntegrationService* const service = GetIntegrationService();
    if (!service) {
      return;
    }

    service->GetSyncingPaths(
        base::BindOnce(&DriveInternalsWebUIHandler::OnGetSyncingPaths,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetSyncingPaths(drive::FileError status,
                         const std::vector<FilePath>& paths) {
    if (status != drive::FILE_ERROR_OK) {
      LOG(ERROR) << "Error retrieving syncing paths: " << status;
      return;
    }
    for (const FilePath& sync_path : paths) {
      MaybeCallJavascript(
          "onAddSyncPath", Value(sync_path.value()),
          Value(drive::FileErrorToString(drive::FILE_ERROR_OK)));
    }
  }

  void ToggleSyncPath(drivefs::mojom::MirrorPathStatus status,
                      const Value::List& args) {
    if (!features::IsDriveFsMirroringEnabled()) {
      return;
    }

    DriveIntegrationService* const service = GetIntegrationService();
    if (!service) {
      return;
    }

    if (args.size() == 1 && args[0].is_string()) {
      const FilePath sync_path(args[0].GetString());
      auto callback =
          base::BindOnce((status == drivefs::mojom::MirrorPathStatus::kStart)
                             ? &DriveInternalsWebUIHandler::OnAddSyncPath
                             : &DriveInternalsWebUIHandler::OnRemoveSyncPath,
                         weak_ptr_factory_.GetWeakPtr(), sync_path);
      service->ToggleSyncForPath(sync_path, status, std::move(callback));
    }
  }

  void OnAddSyncPath(const FilePath& sync_path, drive::FileError status) {
    MaybeCallJavascript("onAddSyncPath", Value(sync_path.value()),
                        Value(drive::FileErrorToString(status)));
  }

  void OnRemoveSyncPath(const FilePath& sync_path, drive::FileError status) {
    MaybeCallJavascript("onRemoveSyncPath", Value(sync_path.value()),
                        Value(drive::FileErrorToString(status)));
  }

  void UpdateBulkPinningDeveloperSection() {
    DriveIntegrationService* const service = GetIntegrationService();
    if (!service) {
      return;
    }

    const bool enabled = drive::util::IsDriveFsBulkPinningEnabled(profile());
    SetSectionEnabled("bulk-pinning-section", enabled);
    if (!enabled) {
      return;
    }

    service->RemoveObserver(this);
    service->AddObserver(this);

    MaybeCallJavascript(
        "updateBulkPinning",
        Value(GetPrefs()->GetBoolean(kDriveFsBulkPinningEnabled)));

    MaybeCallJavascript("onUpdateMaxQueueSize",
                        /*status=*/Value(""),
                        Value(GetPrefs()->GetInteger(
                            drive::prefs::kDriveFsBulkPinningMaxQueueSize)));

    if (PinManager* const manager = service->GetPinManager()) {
      OnBulkPinProgress(manager->GetProgress());
    }
  }

  void OnBulkPinProgress(const drivefs::pinning::Progress& progress) override {
    using drivefs::pinning::HumanReadableSize;

    Value::Dict d;
    d.Set("enabled", GetPrefs()->GetBoolean(kDriveFsBulkPinningEnabled));
    d.Set("stage", drivefs::pinning::ToString(progress.stage));
    d.Set("free_space", ToString(HumanReadableSize(progress.free_space)));
    d.Set("required_space",
          ToString(HumanReadableSize(progress.required_space)));
    d.Set("bytes_to_pin", ToString(HumanReadableSize(progress.bytes_to_pin)));
    d.Set("pinned_bytes", ToString(HumanReadableSize(progress.pinned_bytes)));
    d.Set("pinned_bytes_percent",
          ToPercent(progress.pinned_bytes, progress.bytes_to_pin));
    d.Set("files_to_pin", ToString(progress.files_to_pin));
    d.Set("pinned_files", ToString(progress.pinned_files));
    d.Set("pinned_files_percent",
          ToPercent(progress.pinned_files, progress.files_to_pin));
    d.Set("failed_files", ToString(progress.failed_files));
    d.Set("syncing_files", ToString(progress.syncing_files));
    d.Set("skipped_items", ToString(progress.skipped_items));
    d.Set("listed_items", ToString(progress.listed_items));
    d.Set("listed_dirs", ToString(progress.listed_dirs));
    d.Set("listed_files", ToString(progress.listed_files));
    d.Set("listed_docs", ToString(progress.listed_docs));
    d.Set("listed_shortcuts", ToString(progress.listed_shortcuts));
    d.Set("active_queries", ToString(progress.active_queries));
    d.Set("max_active_queries", ToString(progress.max_active_queries));
    d.Set("time_spent_listing_items",
          drivefs::pinning::ToString(progress.time_spent_listing_items));
    d.Set("time_spent_pinning_files",
          drivefs::pinning::ToString(progress.time_spent_pinning_files));
    d.Set("remaining_time",
          drivefs::pinning::ToString(progress.remaining_time));
    MaybeCallJavascript("onBulkPinningProgress", Value(std::move(d)));
  }

  // Called when GetDeveloperMode() is complete.
  void OnGetDeveloperMode(bool enabled) {
    developer_mode_ = enabled;
    if (!enabled) {
      return;
    }

    RegisterDeveloperMessages();

    // Get the startup arguments.
    if (DriveIntegrationService* const service = GetIntegrationService()) {
      service->GetStartupArguments(
          base::BindOnce(&DriveInternalsWebUIHandler::OnGetStartupArguments,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // Called when GetStartupArguments() is complete.
  void OnGetStartupArguments(const std::string& arguments) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(developer_mode_);
    MaybeCallJavascript("updateStartupArguments", Value(arguments));
    SetSectionEnabled("developer-mode-controls", true);
    UpdateBulkPinningDeveloperSection();
  }

  // Called when AmountOfFreeDiskSpace() is complete.
  void OnGetFreeDiskSpace(int64_t free_space) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    Value::Dict local_storage_summary;
    local_storage_summary.Set("free_space", static_cast<double>(free_space));
    MaybeCallJavascript("updateLocalStorageUsage",
                        Value(std::move(local_storage_summary)));
  }

  void UpdateDriveRelatedPreferencesSection() {
    SetSectionEnabled("drive-related-preferences-section", true);

    const char* const kDriveRelatedPreferences[] = {
        drive::prefs::kDisableDrive,
        drive::prefs::kDisableDriveOverCellular,
        drive::prefs::kDriveFsWasLaunchedAtLeastOnce,
        drive::prefs::kDriveFsPinnedMigrated,
        drive::prefs::kDriveFsEnableVerboseLogging,
        drive::prefs::kDriveFsEnableMirrorSync,
    };

    PrefService* const prefs = GetPrefs();
    Value::List preferences;
    for (const char* key : kDriveRelatedPreferences) {
      // As of now, all preferences are boolean.
      AppendKeyValue(preferences, key,
                     prefs->GetBoolean(key) ? "true" : "false");
    }

    MaybeCallJavascript("updateDriveRelatedPreferences",
                        Value(std::move(preferences)));
  }

  void UpdateEventLogSection() {
    SetSectionEnabled("event-log-section", true);

    DriveIntegrationService* const service = GetIntegrationService();
    if (!service) {
      return;
    }

    const std::vector<drive::EventLogger::Event> log =
        service->event_logger()->GetHistory();

    Value::List list;
    for (const drive::EventLogger::Event& event : log) {
      // Skip events which were already sent.
      if (event.id <= last_sent_event_id_) {
        continue;
      }

      const char* const severity =
          kLogLevelName[SeverityToLogLevelNameIndex(event.severity)];
      AppendKeyValue(list,
                     google_apis::util::FormatTimeAsStringLocaltime(event.when),
                     base::StrCat({"[", severity, "] ", event.what}),
                     base::StrCat({"log-", severity}));
      last_sent_event_id_ = event.id;
    }
    if (!list.empty()) {
      MaybeCallJavascript("updateEventLog", Value(std::move(list)));
    }
  }

  void UpdateServiceLogSection() {
    SetSectionEnabled("service-log-section", true);

    if (service_log_file_is_processing_) {
      return;
    }
    service_log_file_is_processing_ = true;

    DriveIntegrationService* const service = GetIntegrationService();
    if (!service) {
      return;
    }

    FilePath log_path = service->GetDriveFsLogPath();
    if (log_path.empty()) {
      return;
    }

    MaybeCallJavascript(
        "updateOtherServiceLogsUrl",
        Value(net::FilePathToFileURL(log_path.DirName()).spec()));

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&GetServiceLogContents, log_path,
                       service_log_file_inode_, last_sent_line_number_),
        base::BindOnce(&DriveInternalsWebUIHandler::OnServiceLogRead,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Called when service logs are read.
  void OnServiceLogRead(std::pair<ino_t, Value::List> response) {
    if (service_log_file_inode_ != response.first) {
      service_log_file_inode_ = response.first;
      last_sent_line_number_ = 0;
    }
    if (!response.second.empty()) {
      last_sent_line_number_ += response.second.size();
      MaybeCallJavascript("updateServiceLog",
                          Value(std::move(response.second)));
    }
    service_log_file_is_processing_ = false;
  }

  void UpdateCacheContentsSection() {
    // TODO(crbug.com/896123): Maybe worth implementing.
    SetSectionEnabled("cache-contents-section", false);
  }

  void UpdateGCacheContentsSection() {
    SetSectionEnabled("gcache-contents-section", true);

    const FilePath root_path =
        drive::util::GetCacheRootPath(profile()).DirName();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&GetGCacheContents, root_path),
        base::BindOnce(&DriveInternalsWebUIHandler::OnGetGCacheContents,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Called when GetGCacheContents() is complete.
  void OnGetGCacheContents(std::pair<Value::List, Value::Dict> response) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    MaybeCallJavascript("updateGCacheContents",
                        Value(std::move(response.first)),
                        Value(std::move(response.second)));
  }

  // Called when the "Verbose Logging" checkbox on the page is changed.
  void SetVerboseLoggingEnabled(const Value::List& args) {
    AllowJavascript();
    DriveIntegrationService* const service = GetIntegrationService();
    if (!service) {
      return;
    }

    if (args.size() == 1 && args[0].is_bool()) {
      bool enabled = args[0].GetBool();
      GetPrefs()->SetBoolean(drive::prefs::kDriveFsEnableVerboseLogging,
                             enabled);
      RestartDrive(Value::List());
    }
  }

  void SetMirroringEnabled(const Value::List& args) {
    AllowJavascript();
    DriveIntegrationService* const service = GetIntegrationService();
    if (!service) {
      return;
    }

    if (args.size() == 1 && args[0].is_bool()) {
      bool enabled = args[0].GetBool();
      GetPrefs()->SetBoolean(drive::prefs::kDriveFsEnableMirrorSync, enabled);
      SetSectionEnabled("mirror-sync-paths", enabled);
      SetSectionEnabled("mirror-path-form", enabled);
    }
  }

  void SetBulkPinningEnabled(const Value::List& args) {
    AllowJavascript();

    if (args.size() != 1 || !args[0].is_bool()) {
      LOG(ERROR) << "args in not a bool";
      return;
    }

    const bool enabled = args[0].GetBool();
    GetPrefs()->SetBoolean(kDriveFsBulkPinningEnabled, enabled);
    UpdateBulkPinningDeveloperSection();
    drivefs::pinning::RecordBulkPinningEnabledSource(
        drivefs::pinning::BulkPinningEnabledSource::kDriveInternal);
  }

  // Called when the "Startup Arguments" field on the page is submitted.
  void SetStartupArguments(const Value::List& args) {
    AllowJavascript();

    CHECK(developer_mode_);

    if (args.size() < 1 || !args[0].is_string()) {
      OnSetStartupArguments(false);
      return;
    }

    DriveIntegrationService* const service = GetIntegrationService();
    if (!service) {
      OnSetStartupArguments(false);
      return;
    }

    service->SetStartupArguments(
        args[0].GetString(),
        base::BindOnce(&DriveInternalsWebUIHandler::OnSetStartupArguments,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnSetStartupArguments(bool success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(developer_mode_);
    if (success) {
      RestartDrive(Value::List());
    }
    MaybeCallJavascript("updateStartupArgumentsStatus", Value(success));
  }

  void SetTracingEnabled(bool enabled, const Value::List& args) {
    AllowJavascript();
    if (DriveIntegrationService* const service = GetIntegrationService()) {
      service->SetTracingEnabled(enabled);
    }
  }

  void SetNetworkingEnabled(bool enabled, const Value::List& args) {
    AllowJavascript();
    CHECK(developer_mode_);
    if (DriveIntegrationService* const service = GetIntegrationService()) {
      service->SetNetworkingEnabled(enabled);
    }
  }

  void ForcePauseSyncing(bool enabled, const Value::List& args) {
    AllowJavascript();
    CHECK(developer_mode_);
    if (DriveIntegrationService* const service = GetIntegrationService()) {
      service->ForcePauseSyncing(enabled);
    }
  }

  void DumpAccountSettings(const Value::List& args) {
    AllowJavascript();
    CHECK(developer_mode_);
    if (DriveIntegrationService* const service = GetIntegrationService()) {
      service->DumpAccountSettings();
    }
  }

  void LoadAccountSettings(const Value::List& args) {
    AllowJavascript();
    CHECK(developer_mode_);
    if (DriveIntegrationService* const service = GetIntegrationService()) {
      service->LoadAccountSettings();
    }
  }

  // Called when the "Restart Drive" button on the page is pressed.
  void RestartDrive(const Value::List& args) {
    AllowJavascript();

    if (DriveIntegrationService* const service = GetIntegrationService()) {
      service->RestartDrive();
    }
  }

  // Called when the corresponding button on the page is pressed.
  void ResetDriveFileSystem(const Value::List& args) {
    AllowJavascript();

    if (DriveIntegrationService* const service = GetIntegrationService()) {
      service->ClearCacheAndRemountFileSystem(
          base::BindOnce(&DriveInternalsWebUIHandler::ResetFinished,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void ZipDriveFsLogs(const Value::List& args) {
    AllowJavascript();

    DriveIntegrationService* const service = GetIntegrationService();
    if (!service || service->GetDriveFsLogPath().empty()) {
      return;
    }

    ZipLogs(profile(), weak_ptr_factory_.GetWeakPtr());
  }

  // Called after file system reset for ResetDriveFileSystem is done.
  void ResetFinished(bool success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    MaybeCallJavascript("updateResetStatus", Value(success));
  }

  void UpdateBulkPinningMaxQueueSize(const Value::List& args) {
    AllowJavascript();

    if (args.size() != 1 || !args[0].is_int()) {
      MaybeCallJavascript("onUpdateMaxQueueSize", Value("invalid queue size"),
                          Value(GetPrefs()->GetInteger(
                              drive::prefs::kDriveFsBulkPinningMaxQueueSize)));
      return;
    }

    const int max_queue_size = args[0].GetInt();
    if (max_queue_size < 1 || max_queue_size > 200) {
      MaybeCallJavascript("onUpdateMaxQueueSize", Value("invalid queue size"),
                          Value(GetPrefs()->GetInteger(
                              drive::prefs::kDriveFsBulkPinningMaxQueueSize)));
      return;
    }

    GetPrefs()->SetInteger(drive::prefs::kDriveFsBulkPinningMaxQueueSize,
                           max_queue_size);
    MaybeCallJavascript("onUpdateMaxQueueSize", Value("success"),
                        Value(max_queue_size));
  }

  Profile* profile() { return Profile::FromWebUI(web_ui()); }
  PrefService* GetPrefs() { return profile()->GetPrefs(); }

  // Returns a DriveIntegrationService, if any.
  // May return nullptr in guest/incognito mode.
  DriveIntegrationService* GetIntegrationService() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DriveIntegrationService* const service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());

    if (!service) {
      LOG(ERROR) << "No DriveFS integration service";
      return nullptr;
    }

    if (!service->is_enabled()) {
      LOG(ERROR) << "DriveFS integration service is disabled";
      return nullptr;
    }

    return service;
  }

  // The last event sent to the JavaScript side.
  int last_sent_event_id_ = -1;

  // The last line of service log sent to the JS side.
  int last_sent_line_number_;

  // The inode of the log file.
  ino_t service_log_file_inode_;

  // Service log file is being parsed.
  bool service_log_file_is_processing_ = false;

  // Whether developer mode is enabled for debug commands.
  bool developer_mode_ = false;

  base::WeakPtrFactory<DriveInternalsWebUIHandler> weak_ptr_factory_{this};
};

class LogsZipper : public download::AllDownloadItemNotifier::Observer {
 public:
  LogsZipper(Profile* profile,
             base::WeakPtr<DriveInternalsWebUIHandler> drive_internals)
      : profile_(profile),
        logs_directory_(
            drive::DriveIntegrationServiceFactory::FindForProfile(profile)
                ->GetDriveFsLogPath()
                .DirName()),
        zip_path_(logs_directory_.AppendASCII(kLogsZipName)),
        drive_internals_(std::move(drive_internals)) {}

  LogsZipper(const LogsZipper&) = delete;
  LogsZipper& operator=(const LogsZipper&) = delete;

  void Start() {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&LogsZipper::EnumerateLogFiles, logs_directory_,
                       zip_path_),
        base::BindOnce(&LogsZipper::ZipLogFiles, base::Unretained(this)));
  }

 private:
  static constexpr char kLogsZipName[] = "drivefs_logs.zip";

  void ZipLogFiles(const std::vector<FilePath>& files) {
    const scoped_refptr<ZipFileCreator> creator =
        base::MakeRefCounted<ZipFileCreator>(logs_directory_, files, zip_path_);
    creator->SetCompletionCallback(base::BindOnce(
        &LogsZipper::OnZipDone, base::Unretained(this), creator));
    creator->Start(LaunchFileUtilService());
  }

  static std::vector<FilePath> EnumerateLogFiles(FilePath logs_path,
                                                 FilePath zip_path) {
    // Note: this may be racy if multiple attempts to export logs are run
    // concurrently, but it's a debug page and it requires explicit action to
    // cause problems.
    base::DeleteFile(zip_path);
    std::vector<FilePath> log_files;
    FileEnumerator enumerator(logs_path, false /* recursive */,
                              FileEnumerator::FILES);

    for (FilePath current = enumerator.Next(); !current.empty();
         current = enumerator.Next()) {
      if (!current.MatchesExtension(".zip")) {
        log_files.push_back(current.BaseName());
      }
    }
    return log_files;
  }

  void OnZipDone(const scoped_refptr<ZipFileCreator> creator) {
    DCHECK(creator);
    if (!drive_internals_ || creator->GetResult() != ZipFileCreator::kSuccess) {
      CleanUp();
      return;
    }
    download_notifier_ = std::make_unique<download::AllDownloadItemNotifier>(
        profile_->GetDownloadManager(), this);
    drive_internals_->DownloadLogsZip(zip_path_);
  }

  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    if (item->GetState() == download::DownloadItem::IN_PROGRESS ||
        item->GetURL() != net::FilePathToFileURL(zip_path_)) {
      return;
    }
    CleanUp();
  }

  void CleanUp() {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::GetDeleteFileCallback(zip_path_));
    download_notifier_.reset();
    if (drive_internals_) {
      drive_internals_->OnZipDone();
    }
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
  }

  const raw_ptr<Profile, ExperimentalAsh> profile_;
  const FilePath logs_directory_;
  const FilePath zip_path_;

  const base::WeakPtr<DriveInternalsWebUIHandler> drive_internals_;

  std::unique_ptr<download::AllDownloadItemNotifier> download_notifier_;
};

constexpr char LogsZipper::kLogsZipName[];

void ZipLogs(Profile* profile,
             base::WeakPtr<DriveInternalsWebUIHandler> drive_internals) {
  auto* logs_zipper = new LogsZipper(profile, std::move(drive_internals));
  logs_zipper->Start();
}

}  // namespace

DriveInternalsUI::DriveInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<DriveInternalsWebUIHandler>());

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIDriveInternalsHost);
  source->AddResourcePath("drive_internals.css", IDR_DRIVE_INTERNALS_CSS);
  source->AddResourcePath("drive_internals.js", IDR_DRIVE_INTERNALS_JS);
  source->SetDefaultResource(IDR_DRIVE_INTERNALS_HTML);
}

}  // namespace ash
