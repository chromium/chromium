// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_handler.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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
#include "google_apis/common/time_util.h"

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
      base::BindRepeating(
          &SyncFileSystemInternalsHandler::HandleGetServiceStatus,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLog",
      base::BindRepeating(&SyncFileSystemInternalsHandler::HandleGetLog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearLogs",
      base::BindRepeating(&SyncFileSystemInternalsHandler::HandleClearLogs,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNotificationSource",
      base::BindRepeating(
          &SyncFileSystemInternalsHandler::HandleGetNotificationSource,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "observeTaskLog",
      base::BindRepeating(&SyncFileSystemInternalsHandler::HandleObserveTaskLog,
                          base::Unretained(this)));
}

void SyncFileSystemInternalsHandler::OnSyncStateUpdated(
    const GURL& app_origin,
    sync_file_system::SyncServiceState state,
    const std::string& description) {
  if (!IsJavascriptAllowed()) {
    // Javascript is disallowed, either due to the page still loading, or in the
    // process of being unloaded. Skip this update.
    return;
  }
  std::string state_string = chrome_apps::api::sync_file_system::ToString(
      chrome_apps::api::SyncServiceStateToExtensionEnum(state));
  if (!description.empty())
    state_string += " (" + description + ")";

  // TODO(calvinlo): OnSyncStateUpdated should be updated to also provide the
  // notification mechanism (XMPP or Polling).
  FireWebUIListener("service-status-changed", base::Value(state_string));
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
  base::Value::Dict dict;
  int64_t duration = (task_log.end_time - task_log.start_time).InMilliseconds();
  dict.Set("duration", static_cast<int>(duration));
  dict.Set("task_description", task_log.task_description);
  dict.Set("result_description", task_log.result_description);

  base::Value::List details;
  for (const std::string& detail : task_log.details) {
    details.Append(detail);
  }
  dict.Set("details", std::move(details));
  FireWebUIListener("task-log-recorded", dict);
}

void SyncFileSystemInternalsHandler::HandleGetServiceStatus(
    const base::Value::List& args) {
  AllowJavascript();
  SyncServiceState state_enum = sync_file_system::SYNC_SERVICE_DISABLED;
  sync_file_system::SyncFileSystemService* sync_service =
      SyncFileSystemServiceFactory::GetForProfile(profile_);
  if (sync_service)
    state_enum = sync_service->GetSyncServiceState();
  const std::string state_string = chrome_apps::api::sync_file_system::ToString(
      chrome_apps::api::SyncServiceStateToExtensionEnum(state_enum));
  ResolveJavascriptCallback(args[0] /* callback_id */,
                            base::Value(state_string));
}

void SyncFileSystemInternalsHandler::HandleGetNotificationSource(
    const base::Value::List& args) {
  AllowJavascript();
  drive::DriveNotificationManager* drive_notification_manager =
      drive::DriveNotificationManagerFactory::FindForBrowserContext(profile_);
  if (!drive_notification_manager)
    return;
  bool xmpp_enabled = drive_notification_manager->push_notification_enabled();
  std::string notification_source = xmpp_enabled ? "XMPP" : "Polling";
  ResolveJavascriptCallback(args[0] /* callback_id */,
                            base::Value(notification_source));
}

void SyncFileSystemInternalsHandler::HandleGetLog(
    const base::Value::List& args) {
  AllowJavascript();
  DCHECK_GE(args.size(), 1u);
  const base::Value& callback_id = args[0];
  const std::vector<EventLogger::Event> log =
      sync_file_system::util::GetLogHistory();

  int last_log_id_sent = -1;
  if (args.size() >= 2 && args[1].is_int())
    last_log_id_sent = args[1].GetInt();

  // Collate events which haven't been sent to WebUI yet.
  base::Value::List list;
  for (const auto& entry : log) {
    if (entry.id <= last_log_id_sent)
      continue;

    base::Value::Dict dict;
    dict.Set("id", entry.id);
    dict.Set("time",
             google_apis::util::FormatTimeAsStringLocaltime(entry.when));
    dict.Set("logEvent", entry.what);
    list.Append(std::move(dict));
    last_log_id_sent = entry.id;
  }
  if (list.empty())
    return;

  ResolveJavascriptCallback(callback_id, list);
}

void SyncFileSystemInternalsHandler::HandleClearLogs(
    const base::Value::List& args) {
  sync_file_system::util::ClearLog();
}

void SyncFileSystemInternalsHandler::HandleObserveTaskLog(
    const base::Value::List& args) {
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
