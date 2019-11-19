// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYNC_OS_SYNC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYNC_OS_SYNC_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "components/sync/driver/sync_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace base {
class ListValue;
}  // namespace base

namespace syncer {
class SyncService;
class SyncSetupInProgressHandle;
}  // namespace syncer

// WebUI handler for JS/C++ communication for Chrome OS settings sync controls
// page.
class OSSyncHandler : public content::WebUIMessageHandler,
                      public syncer::SyncServiceObserver {
 public:
  explicit OSSyncHandler(Profile* profile);
  ~OSSyncHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* service) override;

  // Callbacks from the page. Visible for testing.
  void HandleDidNavigateToOsSyncPage(const base::ListValue* args);
  void HandleDidNavigateAwayFromOsSyncPage(const base::ListValue* args);
  void HandleSetOsSyncDatatypes(const base::ListValue* args);

  void SetWebUIForTest(content::WebUI* web_ui);

 private:
  // Pushes the updated sync prefs to JavaScript.
  void PushSyncPrefs();

  // Gets the SyncService associated with the parent profile.
  syncer::SyncService* GetSyncService() const;

  void AddSyncServiceObserver();
  void RemoveSyncServiceObserver();

  Profile* const profile_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  DISALLOW_COPY_AND_ASSIGN(OSSyncHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYNC_OS_SYNC_HANDLER_H_
