// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "chrome/browser/sync_file_system/task_logger.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace syncfs_internals {

// This class handles message from WebUI page of chrome://syncfs-internals/
// for the Sync Service tab. It corresponds to browser/resources/
// sync_file_system_internals/sync_service.html. All methods in this class
// should be called on UI thread.
class SyncFileSystemInternalsHandler
    : public content::WebUIMessageHandler,
      public sync_file_system::SyncEventObserver,
      public sync_file_system::TaskLogger::Observer {
 public:
  explicit SyncFileSystemInternalsHandler(Profile* profile);

  SyncFileSystemInternalsHandler(const SyncFileSystemInternalsHandler&) =
      delete;
  SyncFileSystemInternalsHandler& operator=(
      const SyncFileSystemInternalsHandler&) = delete;

  ~SyncFileSystemInternalsHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // sync_file_system::SyncEventObserver interface implementation.
  void OnSyncStateUpdated(const GURL& app_origin,
                          sync_file_system::SyncServiceState state,
                          const std::string& description) override;
  void OnFileSynced(const storage::FileSystemURL& url,
                    sync_file_system::SyncFileType file_type,
                    sync_file_system::SyncFileStatus status,
                    sync_file_system::SyncAction action,
                    sync_file_system::SyncDirection direction) override;

  // sync_file_system::TaskLogger::Observer implementation.
  void OnLogRecorded(
      const sync_file_system::TaskLogger::TaskLog& task_log) override;

 private:
  void HandleGetServiceStatus(const base::Value::List& args);
  void HandleGetNotificationSource(const base::Value::List& args);
  void HandleGetLog(const base::Value::List& args);
  void HandleClearLogs(const base::Value::List& args);
  void HandleObserveTaskLog(const base::Value::List& args);

  raw_ptr<Profile> profile_;
  bool observing_task_log_;
};

}  // namespace syncfs_internals

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_HANDLER_H_
