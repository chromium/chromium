// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_handler.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/apps/platform_apps/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/drive/drive_notification_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/sync_file_system/sync_service_state.h"
#include "chrome/common/apps/platform_apps/api/sync_file_system.h"
#include "components/drive/drive_notification_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/drive/time_util.h"

using drive::EventLogger;
using sync_file_system::SyncFileSystemServiceFactory;
using sync_file_system::SyncServiceState;

namespace syncfs_internals {

SyncFileSystemInternalsHandler::SyncFileSystemInternalsHandler(Profile* profile)
    : profile_(profile),
      observing_task_log_(false) {
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile);
  if (sync_service)
    sync_service->AddSyncEventObserver(this);
}

SyncFileSystemInternalsHandler::~SyncFileSystemInternalsHandler() {
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile_);
  if (!sync_service)
    return;
  sync_service->RemoveSyncEventObserver(this);
  if (observing_task_log_)
    sync_service->task_logger()->RemoveObserver(this);
}

void SyncFileSystemInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getServiceStatus",
      base::BindRepeating(&SyncFileSystemInternalsHandler::GetServiceStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLog", base::BindRepeating(&SyncFileSystemInternalsHandler::GetLog,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearLogs",
      base::BindRepeating(&SyncFileSystemInternalsHandler::ClearLogs,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNotificationSource",
      base::BindRepeating(
          &SyncFileSystemInternalsHandler::GetNotificationSource,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "observeTaskLog",
      base::BindRepeating(&SyncFileSystemInternalsHandler::ObserveTaskLog,
                          base::Unretained(this)));
}

void SyncFileSystemInternalsHandler::OnSyncStateUpdated(
    const GURL& app_origin,
    sync_file_system::SyncServiceState state,
    const std::string& description) {
  std::string state_string = chrome_apps::api::sync_file_system::ToString(
      chrome_apps::api::SyncServiceStateToExtensionEnum(state));
  if (!description.empty())
    state_string += " (" + description + ")";

  // TODO(calvinlo): OnSyncStateUpdated should be updated to also provide the
  // notification mechanism (XMPP or Polling).
  web_ui()->CallJavascriptFunctionUnsafe("SyncService.onGetServiceStatus",
                                         base::Value(state_string));
}

void SyncFileSystemInternalsHandler::OnFileSynced(
    const storage::FileSystemURL& url,
    sync_file_system::SyncFileType file_type,
    sync_file_system::SyncFileStatus status,
    sync_file_system::SyncAction action,
    sync_file_system::SyncDirection direction) {
}

void SyncFileSystemInternalsHandler::OnLogRecorded(
    const sync_file_system::TaskLogger::TaskLog& task_log) {
  base::DictionaryValue dict;
  int64_t duration = (task_log.end_time - task_log.start_time).InMilliseconds();
  dict.SetInteger("duration", duration);
  dict.SetString("task_description", task_log.task_description);
  dict.SetString("result_description", task_log.result_description);

  std::unique_ptr<base::ListValue> details(new base::ListValue);
  details->AppendStrings(task_log.details);
  dict.Set("details", std::move(details));
  web_ui()->CallJavascriptFunctionUnsafe("TaskLog.onTaskLogRecorded", dict);
}

void SyncFileSystemInternalsHandler::GetServiceStatus(
    const base::ListValue* args) {
  SyncServiceState state_enum = sync_file_system::SYNC_SERVICE_DISABLED;
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile_);
  if (sync_service)
    state_enum = sync_service->GetSyncServiceState();
  const std::string state_string = chrome_apps::api::sync_file_system::ToString(
      chrome_apps::api::SyncServiceStateToExtensionEnum(state_enum));
  web_ui()->CallJavascriptFunctionUnsafe("SyncService.onGetServiceStatus",
                                         base::Value(state_string));
}

void SyncFileSystemInternalsHandler::GetNotificationSource(
    const base::ListValue* args) {
  drive::DriveNotificationManager* drive_notification_manager =
      drive::DriveNotificationManagerFactory::FindForBrowserContext(profile_);
  if (!drive_notification_manager)
    return;
  bool xmpp_enabled = drive_notification_manager->push_notification_enabled();
  std::string notification_source = xmpp_enabled ? "XMPP" : "Polling";
  web_ui()->CallJavascriptFunctionUnsafe("SyncService.onGetNotificationSource",
                                         base::Value(notification_source));
}

void SyncFileSystemInternalsHandler::GetLog(
    const base::ListValue* args) {
  const std::vector<EventLogger::Event> log =
      sync_file_system::util::GetLogHistory();

  int last_log_id_sent;
  if (!args->GetInteger(0, &last_log_id_sent))
    last_log_id_sent = -1;

  // Collate events which haven't been sent to WebUI yet.
  base::ListValue list;
  for (auto log_entry = log.begin(); log_entry != log.end(); ++log_entry) {
    if (log_entry->id <= last_log_id_sent)
      continue;

    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    dict->SetInteger("id", log_entry->id);
    dict->SetString("time",
        google_apis::util::FormatTimeAsStringLocaltime(log_entry->when));
    dict->SetString("logEvent", log_entry->what);
    list.Append(std::move(dict));
    last_log_id_sent = log_entry->id;
  }
  if (list.empty())
    return;

  web_ui()->CallJavascriptFunctionUnsafe("SyncService.onGetLog", list);
}

void SyncFileSystemInternalsHandler::ClearLogs(const base::ListValue* args) {
  sync_file_system::util::ClearLog();
}

void SyncFileSystemInternalsHandler::ObserveTaskLog(
    const base::ListValue* args) {
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile_);
  if (!sync_service)
    return;
  if (!observing_task_log_) {
    observing_task_log_ = true;
    sync_service->task_logger()->AddObserver(this);
  }

  DCHECK(sync_service->task_logger());
  const sync_file_system::TaskLogger::LogList& log =
      sync_service->task_logger()->GetLog();

  for (sync_file_system::TaskLogger::LogList::const_iterator itr = log.begin();
       itr != log.end(); ++itr)
    OnLogRecorded(**itr);
}

}  // namespace syncfs_internals
