// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/drive_internals_ui.h"

#include <stddef.h>
#include <stdint.h>

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/services/file_util/public/cpp/zip_file_creator.h"
#include "chromeos/constants/chromeos_features.h"
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
#include "google_apis/drive/auth_service.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/time_util.h"
#include "net/base/filename_util.h"

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
  list->Append(std::move(*dict));
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

class DriveInternalsWebUIHandler;

void ZipLogs(Profile* profile,
             base::WeakPtr<DriveInternalsWebUIHandler> drive_internals);

// Class to handle messages from chrome://drive-internals.
class DriveInternalsWebUIHandler : public content::WebUIMessageHandler {
 public:
  DriveInternalsWebUIHandler() : last_sent_event_id_(-1) {}

  ~DriveInternalsWebUIHandler() override {}

  void DownloadLogsZip(const base::FilePath& path) {
    web_ui()->GetWebContents()->GetController().LoadURL(
        net::FilePathToFileURL(path), {}, {}, {});
  }

  void OnZipDone() { MaybeCallJavascript("onZipDone", base::Value()); }

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
        "resetDriveFileSystem",
        base::BindRepeating(&DriveInternalsWebUIHandler::ResetDriveFileSystem,
                            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "zipLogs",
        base::BindRepeating(&DriveInternalsWebUIHandler::ZipDriveFsLogs,
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
    drive::DriveNotificationManager* drive_notification_manager =
        drive::DriveNotificationManagerFactory::FindForBrowserContext(
            profile());
    connection_status.SetBoolean(
        "push-notification-enabled",
        drive_notification_manager
            ? drive_notification_manager->push_notification_enabled()
            : false);

    MaybeCallJavascript("updateConnectionStatus", std::move(connection_status));
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
      base::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_VISIBLE},
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
        drive::prefs::kDriveFsWasLaunchedAtLeastOnce,
        drive::prefs::kDriveFsPinnedMigrated,
    };

    PrefService* pref_service = profile()->GetPrefs();

    base::ListValue preferences;
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

    MaybeCallJavascript(
        "updateOtherServiceLogsUrl",
        base::Value(net::FilePathToFileURL(log_path.DirName()).spec()));

    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
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
    // TODO(crbug.com/896123): Maybe worth implementing.
    SetSectionEnabled("cache-contents-section", false);
  }

  void UpdateGCacheContentsSection() {
    SetSectionEnabled("gcache-contents-section", true);

    const base::FilePath root_path =
        drive::util::GetCacheRootPath(profile()).DirName();
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
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

  void ZipDriveFsLogs(const base::ListValue* args) {
    AllowJavascript();

    drive::DriveIntegrationService* integration_service =
        GetIntegrationService();
    if (!integration_service ||
        integration_service->GetDriveFsLogPath().empty()) {
      return;
    }

    ZipLogs(profile(), weak_ptr_factory_.GetWeakPtr());
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

  // The last event sent to the JavaScript side.
  int last_sent_event_id_;

  // The last line of service log sent to the JS side.
  int last_sent_line_number_;

  // The inode of the log file.
  ino_t service_log_file_inode_;

  // Service log file is being parsed.
  bool service_log_file_is_processing_ = false;

  base::WeakPtrFactory<DriveInternalsWebUIHandler> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DriveInternalsWebUIHandler);
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

  void Start() {
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&LogsZipper::EnumerateLogFiles, logs_directory_,
                       zip_path_),
        base::BindOnce(&LogsZipper::ZipLogFiles, base::Unretained(this)));
  }

 private:
  static constexpr char kLogsZipName[] = "drivefs_logs.zip";

  void ZipLogFiles(const std::vector<base::FilePath>& files) {
    (new ZipFileCreator(
         base::BindRepeating(&LogsZipper::OnZipDone, base::Unretained(this)),
         logs_directory_, files, zip_path_))
        ->Start(LaunchFileUtilService());
  }

  static std::vector<base::FilePath> EnumerateLogFiles(
      base::FilePath logs_path,
      base::FilePath zip_path) {
    // Note: this may be racy if multiple attempts to export logs are run
    // concurrently, but it's a debug page and it requires explicit action to
    // cause problems.
    base::DeleteFile(zip_path, false);
    std::vector<base::FilePath> log_files;
    base::FileEnumerator enumerator(logs_path, false /* recursive */,
                                    base::FileEnumerator::FILES);

    for (base::FilePath current = enumerator.Next(); !current.empty();
         current = enumerator.Next()) {
      if (!current.MatchesExtension(".zip")) {
        log_files.push_back(current.BaseName());
      }
    }
    return log_files;
  }

  void OnZipDone(bool success) {
    if (!drive_internals_ || !success) {
      CleanUp();
      return;
    }
    download_notifier_ = std::make_unique<download::AllDownloadItemNotifier>(
        content::BrowserContext::GetDownloadManager(profile_), this);
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
    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), zip_path_,
                       false));
    download_notifier_.reset();
    if (drive_internals_) {
      drive_internals_->OnZipDone();
    }
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  Profile* const profile_;
  const base::FilePath logs_directory_;
  const base::FilePath zip_path_;

  const base::WeakPtr<DriveInternalsWebUIHandler> drive_internals_;

  std::unique_ptr<download::AllDownloadItemNotifier> download_notifier_;

  DISALLOW_COPY_AND_ASSIGN(LogsZipper);
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

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDriveInternalsHost);
  source->AddResourcePath("drive_internals.css", IDR_DRIVE_INTERNALS_CSS);
  source->AddResourcePath("drive_internals.js", IDR_DRIVE_INTERNALS_JS);
  source->SetDefaultResource(IDR_DRIVE_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source);
}

}  // namespace chromeos
