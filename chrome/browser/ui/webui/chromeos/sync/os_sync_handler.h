// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYNC_OS_SYNC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYNC_OS_SYNC_HANDLER_H_

#include "base/macros.h"
#include "components/sync/driver/sync_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace base {
class ListValue;
}  // namespace base

namespace syncer {
class SyncService;
}  // namespace syncer

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
  void HandleDidNavigateToOsSyncPage(const base::ListValue* args);
  void HandleDidNavigateAwayFromOsSyncPage(const base::ListValue* args);
  void HandleOsSyncPrefsDispatch(const base::ListValue* args);
  void HandleSetOsSyncFeatureEnabled(const base::ListValue* args);
  void HandleSetOsSyncDatatypes(const base::ListValue* args);

  void SetWebUIForTest(content::WebUI* web_ui);

 private:
  // Sets the OS sync feature enabled pref if the user changed the setting.
  void CommitFeatureEnabledPref();

  // Pushes the updated sync prefs to JavaScript.
  void PushSyncPrefs();

  // Gets the SyncService associated with the parent profile.
  syncer::SyncService* GetSyncService() const;

  void AddSyncServiceObserver();
  void RemoveSyncServiceObserver();

  Profile* const profile_;

  // Cached copy of the OS sync feature enabled pref. Used to avoid turning on
  // OS sync before the user is done configuring the toggles.
  bool feature_enabled_ = false;

  // Whether to commit the feature enabled state when the user closes the UI.
  bool should_commit_feature_enabled_ = false;

  // Prevents messages to JS layer while data type prefs are being set.
  bool is_setting_prefs_ = false;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SYNC_OS_SYNC_HANDLER_H_
