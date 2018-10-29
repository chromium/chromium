// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/drive_internals_ui.h"

#include <stddef.h>
#include <stdint.h>

#include <fstream>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/drive/debug_info_collector.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/chromeos_features.h"
#include "components/drive/drive.pb.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/event_logger.h"
#include "components/drive/job_list.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/drive/auth_service.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/time_util.h"

using content::BrowserThread;

namespace chromeos {

namespace {

constexpr char kKey[] = "key";
constexpr char kValue[] = "value";
constexpr char kClass[] = "class";

constexpr const char* const kLogLevelName[] = {"info", "warning", "error"};

size_t SeverityToLogLevelNameIndex(logging::LogSeverity severity) {
  if (severity <= logging::LOG_INFO)
    return 0;
  if (severity == logging::LOG_WARNING)
    return 1;
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
std::pair<base::ListValue, base::DictionaryValue> GetGCacheContents(
    const base::FilePath& root_path) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Use this map to sort the result list by the path.
  std::map<base::FilePath, std::unique_ptr<base::DictionaryValue>> files;

  const int options =
      (base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
       base::FileEnumerator::SHOW_SYM_LINKS);
  base::FileEnumerator enumerator(root_path, true /* recursive */, options);

  int64_t total_size = 0;
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    int64_t size = info.GetSize();
    const bool is_directory = info.IsDirectory();
    const bool is_symbolic_link = base::IsLink(info.GetName());
    const base::Time last_modified = info.GetLastModifiedTime();

    auto entry = std::make_unique<base::DictionaryValue>();
    entry->SetString("path", current.value());
    // Use double instead of integer for large files.
    entry->SetDouble("size", size);
    entry->SetBoolean("is_directory", is_directory);
    entry->SetBoolean("is_symbolic_link", is_symbolic_link);
    entry->SetString(
        "last_modified",
        google_apis::util::FormatTimeAsStringLocaltime(last_modified));
    // Print lower 9 bits in octal format.
    entry->SetString("permission",
                     base::StringPrintf("%03o", info.stat().st_mode & 0x1ff));
    files[current] = std::move(entry);

    total_size += size;
  }

  std::pair<base::ListValue, base::DictionaryValue> result;
  // Convert |files| into response.
  for (auto& it : files)
    result.first.Append(std::move(it.second));
  result.second.SetDouble("total_size", total_size);
  return result;
}

// Formats |entry| into text.
std::string FormatEntry(const base::FilePath& path,
                        const drive::ResourceEntry& entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using base::StringAppendF;

  std::string out;
  StringAppendF(&out, "%s\n", path.AsUTF8Unsafe().c_str());
  StringAppendF(&out, "  title: %s\n", entry.title().c_str());
  StringAppendF(&out, "  local_id: %s\n", entry.local_id().c_str());
  StringAppendF(&out, "  resource_id: %s\n", entry.resource_id().c_str());
  StringAppendF(&out, "  parent_local_id: %s\n",
                entry.parent_local_id().c_str());
  StringAppendF(&out, "  shared: %s\n", entry.shared() ? "true" : "false");
  StringAppendF(&out, "  shared_with_me: %s\n",
                entry.shared_with_me() ? "true" : "false");
  StringAppendF(&out, "  alternate_url: %s\n", entry.alternate_url().c_str());

  const drive::PlatformFileInfoProto& file_info = entry.file_info();
  StringAppendF(&out, "  file_info\n");
  StringAppendF(&out, "    size: %" PRId64 "\n", file_info.size());
  StringAppendF(&out, "    is_directory: %d\n", file_info.is_directory());
  StringAppendF(&out, "    is_symbolic_link: %d\n",
                file_info.is_symbolic_link());

  const base::Time last_modified = base::Time::FromInternalValue(
      file_info.last_modified());
  const base::Time last_modified_by_me =
      base::Time::FromInternalValue(entry.last_modified_by_me());
  const base::Time last_accessed = base::Time::FromInternalValue(
      file_info.last_accessed());
  const base::Time creation_time = base::Time::FromInternalValue(
      file_info.creation_time());
  StringAppendF(&out, "    last_modified: %s\n",
                google_apis::util::FormatTimeAsString(last_modified).c_str());
  StringAppendF(
      &out, "    last_modified_by_me: %s\n",
      google_apis::util::FormatTimeAsString(last_modified_by_me).c_str());
  StringAppendF(&out, "    last_accessed: %s\n",
                google_apis::util::FormatTimeAsString(last_accessed).c_str());
  StringAppendF(&out, "    creation_time: %s\n",
                google_apis::util::FormatTimeAsString(creation_time).c_str());

  if (entry.has_file_specific_info()) {
    const drive::FileSpecificInfo& file_specific_info =
        entry.file_specific_info();
    StringAppendF(&out, "    content_mime_type: %s\n",
                  file_specific_info.content_mime_type().c_str());
    StringAppendF(&out, "    file_md5: %s\n",
                  file_specific_info.md5().c_str());
    StringAppendF(&out, "    document_extension: %s\n",
                  file_specific_info.document_extension().c_str());
    StringAppendF(&out, "    is_hosted_document: %d\n",
                  file_specific_info.is_hosted_document());
  }

  if (entry.has_directory_specific_info()) {
    StringAppendF(&out, "  directory_info\n");
    const drive::DirectorySpecificInfo& directory_specific_info =
        entry.directory_specific_info();
    StringAppendF(&out, "    changestamp: %" PRId64 "\n",
                  directory_specific_info.changestamp());
  }

  return out;
}

// Appends {'key': key, 'value': value, 'class': clazz} dictionary to the
// |list|.
void AppendKeyValue(base::ListValue* list,
                    std::string key,
                    std::string value,
                    std::string clazz = std::string()) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetKey(kKey, base::Value(std::move(key)));
  dict->SetKey(kValue, base::Value(std::move(value)));
  if (!clazz.empty())
    dict->SetKey(kClass, base::Value(std::move(clazz)));
  list->GetList().push_back(std::move(*dict));
}

ino_t GetInodeValue(const base::FilePath& path) {
  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0)
    return 0;
  return file_stats.st_ino;
}

std::pair<ino_t, base::ListValue> GetServiceLogContents(
    const base::FilePath& log_path,
    ino_t inode,
    int from_line_number) {
  base::ListValue result;

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

      AppendKeyValue(&result,
                     google_apis::util::FormatTimeAsStringLocaltime(time),
                     base::StrCat({"[", severity, "] ", line}),
                     base::StrCat({"log-", severity}));
    }
  }

  return {inode, std::move(result)};
}

// Class to handle messages from chrome://drive-internals.
class DriveInternalsWebUIHandler : public content::WebUIMessageHandler {
 public:
  DriveInternalsWebUIHandler()
      : last_sent_event_id_(-1), weak_ptr_factory_(this) {}

  ~DriveInternalsWebUIHandler() override {}

 private:
  void MaybeCallJavascript(const std::string& function,
                           base::Value data1,
                           base::Value data2 = {}) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (IsJavascriptAllowed()) {
      CallJavascriptFunction(function, std::move(data1), std::move(data2));
    }
  }

  // Hide or show a section of the page.
  void SetSectionEnabled(const std::string& section, bool enable) {
    MaybeCallJavascript("setSectionEnabled", base::Value(section),
                        base::Value(enable));
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
        "clearAccessToken",
        base::BindRepeating(&DriveInternalsWebUIHandler::ClearAccessToken,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "clearRefreshToken",
        base::BindRepeating(&DriveInternalsWebUIHandler::ClearRefreshToken,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "resetDriveFileSystem",
        base::BindRepeating(&DriveInternalsWebUIHandler::ResetDriveFileSystem,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "listFileEntries",
        base::BindRepeating(&DriveInternalsWebUIHandler::ListFileEntries,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  // Called when the page is first loaded.
  void OnPageLoaded(const base::ListValue* args) {
    AllowJavascript();

    drive::DriveIntegrationService* integration_service =
        GetIntegrationService();
    // |integration_service| may be NULL in the guest/incognito mode.
    if (!integration_service)
      return;

    UpdateDriveRelatedPreferencesSection();
    UpdateGCacheContentsSection();
    UpdateLocalStorageUsageSection();
    UpdatePathConfigurationsSection();

    UpdateConnectionStatusSection();
    UpdateAboutResourceSection();

    UpdateDeltaUpdateStatusSection();
    UpdateCacheContentsSection();

    UpdateInFlightOperationsSection();

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
  void OnPeriodicUpdate(const base::ListValue* args) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    drive::DriveIntegrationService* integration_service =
        GetIntegrationService();
    // |integration_service| may be NULL in the guest/incognito mode.
    if (!integration_service)
      return;

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

    base::DictionaryValue connection_status;
    connection_status.SetString("status", status);
    auto* drive_service = GetDriveService();
    if (drive_service) {
      connection_status.SetBoolean("has-refresh-token",
                                   drive_service->HasRefreshToken());
      connection_status.SetBoolean("has-access-token",
                                   drive_service->HasAccessToken());
    }

    MaybeCallJavascript("updateConnectionStatus", std::move(connection_status));
  }

  void UpdateAboutResourceSection() {
    if (IsDriveFsEnabled()) {
      // TODO(crbug.com/896123): Maybe worth implementing.
      SetSectionEnabled("account-information-section", false);
      return;
    }

    SetSectionEnabled("account-information-section", true);
    auto* drive_service = GetDriveService();
    if (drive_service) {
      drive_service->GetAboutResource(
          base::Bind(&DriveInternalsWebUIHandler::OnGetAboutResource,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // Called when GetAboutResource() call to DriveService is complete.
  void OnGetAboutResource(
      google_apis::DriveApiErrorCode status,
      std::unique_ptr<google_apis::AboutResource> about_resource) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (status != google_apis::HTTP_SUCCESS) {
      LOG(ERROR) << "Failed to get about resource";
      return;
    }
    DCHECK(about_resource);

    base::DictionaryValue data;
    data.SetDouble("account-quota-total", about_resource->quota_bytes_total());
    data.SetDouble("account-quota-used",
                   about_resource->quota_bytes_used_aggregate());
    data.SetDouble("account-largest-changestamp-remote",
                   about_resource->largest_change_id());
    data.SetString("root-resource-id", about_resource->root_folder_id());

    MaybeCallJavascript("updateAboutResource", std::move(data));
  }

  void UpdateDeltaUpdateStatusSection() {
    if (IsDriveFsEnabled()) {
      // TODO(crbug.com/896123): Maybe worth implementing.
      SetSectionEnabled("delta-update-status-section", false);
      return;
    }

    SetSectionEnabled("delta-update-status-section", true);
    auto* debug_info_collector = GetDebugInfoCollector();
    if (debug_info_collector) {
      debug_info_collector->GetMetadata(base::Bind(
          &DriveInternalsWebUIHandler::OnGetFilesystemMetadataForDeltaUpdate,
          weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // Callback for DebugInfoCollector::GetMetadata for delta update.
  void OnGetFilesystemMetadataForDeltaUpdate(
      const drive::FileSystemMetadata& metadata,
      const std::map<std::string, drive::FileSystemMetadata>&
          team_drive_metadata) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    drive::DriveNotificationManager* drive_notification_manager =
        drive::DriveNotificationManagerFactory::FindForBrowserContext(
            profile());
    if (!drive_notification_manager)
      return;

    base::DictionaryValue delta_update_status;
    delta_update_status.SetBoolean(
        "push-notification-enabled",
        drive_notification_manager->push_notification_enabled());

    auto items = std::make_unique<base::ListValue>();
    // Users default corpus first.
    auto app_data = std::make_unique<base::DictionaryValue>();
    app_data->SetString("id", "default corpus");
    app_data->SetString("root_entry_path", metadata.path);
    app_data->SetString("start_page_token", metadata.start_page_token);
    app_data->SetString("last_check_time",
                        google_apis::util::FormatTimeAsStringLocaltime(
                            metadata.last_update_check_time));
    app_data->SetString(
        "last_check_result",
        drive::FileErrorToString(metadata.last_update_check_error));
    app_data->SetString("refreshing", metadata.refreshing ? "Yes" : "No");

    items->Append(std::move(app_data));

    for (const auto& team_drive : team_drive_metadata) {
      app_data = std::make_unique<base::DictionaryValue>();
      app_data->SetString("id", team_drive.first);
      app_data->SetString("root_entry_path", team_drive.second.path);
      app_data->SetString("start_page_token",
                          team_drive.second.start_page_token);
      app_data->SetString("last_check_time",
                          google_apis::util::FormatTimeAsStringLocaltime(
                              team_drive.second.last_update_check_time));
      app_data->SetString(
          "last_check_result",
          drive::FileErrorToString(team_drive.second.last_update_check_error));
      app_data->SetString("refreshing",
                          team_drive.second.refreshing ? "Yes" : "No");
      items->Append(std::move(app_data));
    }

    delta_update_status.Set("items", std::move(items));

    MaybeCallJavascript("updateDeltaUpdateStatus",
                        std::move(delta_update_status));
  }

  void UpdateInFlightOperationsSection() {
    if (IsDriveFsEnabled()) {
      // TODO(crbug.com/896123): Maybe worth implementing.
      SetSectionEnabled("in-flight-operations-section", false);
      return;
    }

    SetSectionEnabled("in-flight-operations-section", true);
    auto* integration_service = GetIntegrationService();
    if (!integration_service)
      return;
    drive::JobListInterface* job_list = integration_service->job_list();
    if (!job_list)
      return;
    std::vector<drive::JobInfo> info_list = job_list->GetJobInfoList();

    base::ListValue in_flight_operations;
    for (size_t i = 0; i < info_list.size(); ++i) {
      const drive::JobInfo& info = info_list[i];

      auto dict = std::make_unique<base::DictionaryValue>();
      dict->SetInteger("id", info.job_id);
      dict->SetString("type", drive::JobTypeToString(info.job_type));
      dict->SetString("file_path", info.file_path.AsUTF8Unsafe());
      dict->SetString("state", drive::JobStateToString(info.state));
      dict->SetDouble("progress_current", info.num_completed_bytes);
      dict->SetDouble("progress_total", info.num_total_bytes);
      in_flight_operations.Append(std::move(dict));
    }

    MaybeCallJavascript("updateInFlightOperations",
                        std::move(in_flight_operations));
  }

  void UpdateFileSystemContentsSection() {
    if (IsDriveFsEnabled()) {
      SetSectionEnabled("file-system-contents-section", false);
      return;
    }

    SetSectionEnabled("file-system-contents-section", true);
    drive::DebugInfoCollector* debug_info_collector = GetDebugInfoCollector();
    if (!debug_info_collector)
      return;

    // Start rendering the file system tree as text.
    const base::FilePath root_path = drive::util::GetDriveGrandRootPath();

    debug_info_collector->GetResourceEntry(
        root_path,
        base::BindOnce(&DriveInternalsWebUIHandler::OnGetResourceEntryByPath,
                       weak_ptr_factory_.GetWeakPtr(), root_path));

    debug_info_collector->ReadDirectory(
        root_path,
        base::Bind(&DriveInternalsWebUIHandler::OnReadDirectoryByPath,
                   weak_ptr_factory_.GetWeakPtr(), root_path));
  }

  // Called when GetResourceEntryByPath() is complete.
  void OnGetResourceEntryByPath(const base::FilePath& path,
                                drive::FileError error,
                                std::unique_ptr<drive::ResourceEntry> entry) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (error == drive::FILE_ERROR_OK) {
      DCHECK(entry.get());
      MaybeCallJavascript("updateFileSystemContents",
                          base::Value(FormatEntry(path, *entry) + "\n"));
    }
  }

  // Called when ReadDirectoryByPath() is complete.
  void OnReadDirectoryByPath(
      const base::FilePath& parent_path,
      drive::FileError error,
      std::unique_ptr<drive::ResourceEntryVector> entries) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (error == drive::FILE_ERROR_OK) {
      DCHECK(entries.get());

      drive::DebugInfoCollector* debug_info_collector = GetDebugInfoCollector();
      std::string file_system_as_text;
      for (size_t i = 0; i < entries->size(); ++i) {
        const drive::ResourceEntry& entry = (*entries)[i];
        const base::FilePath current_path = parent_path.Append(
            base::FilePath::FromUTF8Unsafe(entry.base_name()));

        file_system_as_text.append(FormatEntry(current_path, entry) + "\n");

        if (entry.file_info().is_directory()) {
          debug_info_collector->ReadDirectory(
              current_path,
              base::Bind(&DriveInternalsWebUIHandler::OnReadDirectoryByPath,
                         weak_ptr_factory_.GetWeakPtr(), current_path));
        }
      }

      // There may be pending ReadDirectoryByPath() calls, but we can update
      // the page with what we have now. This results in progressive
      // updates, which is good for a large file system.
      MaybeCallJavascript("updateFileSystemContents",
                          base::Value(file_system_as_text));
    }
  }

  void UpdatePathConfigurationsSection() {
    SetSectionEnabled("path-configurations-section", true);

    base::ListValue paths;
    AppendKeyValue(&paths, "Downloads",
                   file_manager::util::GetDownloadsFolderForProfile(profile())
                       .AsUTF8Unsafe());
    const auto* integration_service = GetIntegrationService();
    if (integration_service) {
      AppendKeyValue(&paths, "Drive",
                     integration_service->GetMountPointPath().AsUTF8Unsafe());
    }

    const char* kPathPreferences[] = {
        prefs::kSelectFileLastDirectory, prefs::kSaveFileDefaultDirectory,
        prefs::kDownloadDefaultDirectory,
    };
    for (size_t i = 0; i < base::size(kPathPreferences); ++i) {
      const char* const key = kPathPreferences[i];
      AppendKeyValue(&paths, key,
                     profile()->GetPrefs()->GetFilePath(key).AsUTF8Unsafe());
    }

    MaybeCallJavascript("updatePathConfigurations", std::move(paths));
  }

  void UpdateLocalStorageUsageSection() {
    SetSectionEnabled("local-metadata-section", true);

    // Propagate the amount of local free space in bytes.
    base::FilePath home_path;
    if (base::PathService::Get(base::DIR_HOME, &home_path)) {
      base::PostTaskWithTraitsAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, home_path),
          base::BindOnce(&DriveInternalsWebUIHandler::OnGetFreeDiskSpace,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      LOG(ERROR) << "Home directory not found";
    }
  }

  // Called when AmountOfFreeDiskSpace() is complete.
  void OnGetFreeDiskSpace(int64_t free_space) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::DictionaryValue local_storage_summary;
    local_storage_summary.SetDouble("free_space", free_space);
    MaybeCallJavascript("updateLocalStorageUsage",
                        std::move(local_storage_summary));
  }

  void UpdateDriveRelatedPreferencesSection() {
    SetSectionEnabled("drive-related-preferences-section", true);

    const char* kDriveRelatedPreferences[] = {
        drive::prefs::kDisableDrive,
        drive::prefs::kDisableDriveOverCellular,
        drive::prefs::kDisableDriveHostedFiles,
        drive::prefs::kDriveFsWasLaunchedAtLeastOnce,
        drive::prefs::kDriveFsPinnedMigrated,
    };

    PrefService* pref_service = profile()->GetPrefs();

    base::ListValue preferences;
    AppendKeyValue(&preferences, "DriveFS",
                   IsDriveFsEnabled() ? "true" : "false");
    for (size_t i = 0; i < base::size(kDriveRelatedPreferences); ++i) {
      const std::string key = kDriveRelatedPreferences[i];
      // As of now, all preferences are boolean.
      const std::string value =
          (pref_service->GetBoolean(key.c_str()) ? "true" : "false");
      AppendKeyValue(&preferences, key, value);
    }

    MaybeCallJavascript("updateDriveRelatedPreferences",
                        std::move(preferences));
  }

  void UpdateEventLogSection() {
    SetSectionEnabled("event-log-section", true);

    drive::DriveIntegrationService* integration_service =
        GetIntegrationService();
    if (!integration_service)
      return;

    const std::vector<drive::EventLogger::Event> log =
        integration_service->event_logger()->GetHistory();

    base::ListValue list;
    for (size_t i = 0; i < log.size(); ++i) {
      // Skip events which were already sent.
      if (log[i].id <= last_sent_event_id_)
        continue;

      const char* const severity =
          kLogLevelName[SeverityToLogLevelNameIndex(log[i].severity)];
      AppendKeyValue(
          &list, google_apis::util::FormatTimeAsStringLocaltime(log[i].when),
          base::StrCat({"[", severity, "] ", log[i].what}),
          base::StrCat({"log-", severity}));
      last_sent_event_id_ = log[i].id;
    }
    if (!list.empty()) {
      MaybeCallJavascript("updateEventLog", std::move(list));
    }
  }

  void UpdateServiceLogSection() {
    if (!IsDriveFsEnabled()) {
      SetSectionEnabled("service-log-section", false);
      return;
    }
    SetSectionEnabled("service-log-section", true);

    if (service_log_file_is_processing_)
      return;
    service_log_file_is_processing_ = true;

    drive::DriveIntegrationService* integration_service =
        GetIntegrationService();
    if (!integration_service)
      return;
    base::FilePath log_path = integration_service->GetDriveFsLogPath();
    if (log_path.empty())
      return;

    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&GetServiceLogContents, log_path,
                       service_log_file_inode_, last_sent_line_number_),
        base::BindOnce(&DriveInternalsWebUIHandler::OnServiceLogRead,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Called when service logs are read.
  void OnServiceLogRead(std::pair<ino_t, base::ListValue> response) {
    if (service_log_file_inode_ != response.first) {
      service_log_file_inode_ = response.first;
      last_sent_line_number_ = 0;
    }
    if (!response.second.empty()) {
      last_sent_line_number_ += response.second.GetList().size();
      MaybeCallJavascript("updateServiceLog", std::move(response.second));
    }
    service_log_file_is_processing_ = false;
  }

  void UpdateCacheContentsSection() {
    if (IsDriveFsEnabled()) {
      // TODO(crbug.com/896123): Maybe worth implementing.
      SetSectionEnabled("cache-contents-section", false);
      return;
    }
    SetSectionEnabled("cache-contents-section", true);

    auto* debug_info_collector = GetDebugInfoCollector();
    if (debug_info_collector) {
      debug_info_collector->IterateFileCache(
          base::Bind(&DriveInternalsWebUIHandler::OnUpdateCacheEntry,
                     weak_ptr_factory_.GetWeakPtr()),
          base::DoNothing());
    }
  }

  // Called as the iterator for DebugInfoCollector::IterateFileCache().
  void OnUpdateCacheEntry(const std::string& local_id,
                          const drive::FileCacheEntry& cache_entry) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Convert |cache_entry| into a dictionary.
    base::DictionaryValue value;
    value.SetString("local_id", local_id);
    value.SetString("md5", cache_entry.md5());
    value.SetBoolean("is_present", cache_entry.is_present());
    value.SetBoolean("is_pinned", cache_entry.is_pinned());
    value.SetBoolean("is_dirty", cache_entry.is_dirty());

    MaybeCallJavascript("updateCacheContents", std::move(value));
  }

  void UpdateGCacheContentsSection() {
    SetSectionEnabled("gcache-contents-section", true);

    const base::FilePath root_path =
        drive::util::GetCacheRootPath(profile()).DirName();
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&GetGCacheContents, root_path),
        base::BindOnce(&DriveInternalsWebUIHandler::OnGetGCacheContents,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Called when GetGCacheContents() is complete.
  void OnGetGCacheContents(
      std::pair<base::ListValue, base::DictionaryValue> response) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    MaybeCallJavascript("updateGCacheContents", std::move(response.first),
                        std::move(response.second));
  }

  // Called when the corresponding button on the page is pressed.
  void ClearAccessToken(const base::ListValue* args) {
    AllowJavascript();

    drive::DriveServiceInterface* drive_service = GetDriveService();
    if (drive_service)
      drive_service->ClearAccessToken();
  }

  // Called when the corresponding button on the page is pressed.
  void ClearRefreshToken(const base::ListValue* args) {
    AllowJavascript();

    drive::DriveServiceInterface* drive_service = GetDriveService();
    if (drive_service)
      drive_service->ClearRefreshToken();
  }

  // Called when the corresponding button on the page is pressed.
  void ResetDriveFileSystem(const base::ListValue* args) {
    AllowJavascript();

    drive::DriveIntegrationService* integration_service =
        GetIntegrationService();
    if (integration_service) {
      integration_service->ClearCacheAndRemountFileSystem(
          base::Bind(&DriveInternalsWebUIHandler::ResetFinished,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // Called when the corresponding button on the page is pressed.
  void ListFileEntries(const base::ListValue* args) {
    AllowJavascript();

    UpdateFileSystemContentsSection();
  }

  // Called after file system reset for ResetDriveFileSystem is done.
  void ResetFinished(bool success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    MaybeCallJavascript("updateResetStatus", base::Value(success));
  }

  Profile* profile() { return Profile::FromWebUI(web_ui()); }

  // Returns a DriveIntegrationService.
  drive::DriveIntegrationService* GetIntegrationService() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    drive::DriveIntegrationService* service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    if (!service || !service->is_enabled())
      return NULL;
    return service;
  }

  // Returns a DriveService instance.
  drive::DriveServiceInterface* GetDriveService() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return drive::util::GetDriveServiceByProfile(profile());
  }

  // Returns a DebugInfoCollector instance.
  drive::DebugInfoCollector* GetDebugInfoCollector() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    drive::DriveIntegrationService* service = GetIntegrationService();
    return service ? service->debug_info_collector() : NULL;
  }

  // Whether DriveFS is enabled.
  bool IsDriveFsEnabled() {
    return base::FeatureList::IsEnabled(chromeos::features::kDriveFs);
  }

  // The last event sent to the JavaScript side.
  int last_sent_event_id_;

  // The last line of service log sent to the JS side.
  int last_sent_line_number_;

  // The inode of the log file.
  ino_t service_log_file_inode_;

  // Service log file is being parsed.
  bool service_log_file_is_processing_ = false;

  base::WeakPtrFactory<DriveInternalsWebUIHandler> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveInternalsWebUIHandler);
};

}  // namespace

DriveInternalsUI::DriveInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<DriveInternalsWebUIHandler>());

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDriveInternalsHost);
  source->AddResourcePath("drive_internals.css", IDR_DRIVE_INTERNALS_CSS);
  source->AddResourcePath("drive_internals.js", IDR_DRIVE_INTERNALS_JS);
  source->SetDefaultResource(IDR_DRIVE_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source);
}

}  // namespace chromeos
