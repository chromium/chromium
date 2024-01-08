// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_OS_SYNC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_OS_SYNC_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/sync/service/sync_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ash {

// WebUI handler for JS/C++ communication for Chrome OS settings sync controls
// page.
class OSSyncHandler : public content::WebUIMessageHandler,
                      public syncer::SyncServiceObserver {
 public:
  explicit OSSyncHandler(Profile* profile);

  OSSyncHandler(const OSSyncHandler&) = delete;
  OSSyncHandler& operator=(const OSSyncHandler&) = delete;

  ~OSSyncHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* service) override;

  // Callbacks from the page. Visible for testing.
  void HandleDidNavigateToOsSyncPage(const base::Value::List& args);
  void HandleDidNavigateAwayFromOsSyncPage(const base::Value::List& args);
  void HandleOsSyncPrefsDispatch(const base::Value::List& args);
  void HandleSetOsSyncDatatypes(const base::Value::List& args);
  void HandleOpenBrowserSyncSettings(const base::Value::List& args);

 private:
  // Pushes the updated sync prefs to JavaScript.
  void PushSyncPrefs();

  // Gets the SyncService associated with the parent profile.
  syncer::SyncService* GetSyncService() const;

  void AddSyncServiceObserver();
  void RemoveSyncServiceObserver();

  const raw_ptr<Profile> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_OS_SYNC_HANDLER_H_
