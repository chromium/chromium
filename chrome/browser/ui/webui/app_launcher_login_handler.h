// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_LOGIN_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;
class ProfileInfoWatcher;

// The login handler currently simply displays the current logged in
// username at the top of the NTP (and update itself when that changes).
// In the future it may expand to allow users to login from the NTP.
class AppLauncherLoginHandler : public content::WebUIMessageHandler {
 public:
  AppLauncherLoginHandler();
  ~AppLauncherLoginHandler() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // Returns true if the login handler should be shown in a new tab page
  // for the given |profile|. |profile| must not be NULL.
  static bool ShouldShow(Profile* profile);

 private:
  // User actions while on the NTP when clicking on or viewing the sync promo.
  enum NTPSignInPromoBuckets {
    NTP_SIGN_IN_PROMO_VIEWED,
    NTP_SIGN_IN_PROMO_CLICKED,
    NTP_SIGN_IN_PROMO_BUCKET_BOUNDARY,
  };

  // Called from JS when the NTP is loaded. |args| is the list of arguments
  // passed from JS and should be an empty list.
  void HandleInitializeSyncLogin(const base::ListValue* args);

#if !defined(OS_CHROMEOS)
  // Called from JS when the user clicks the login container. It shows the
  // appropriate UI based on the current sync state. |args| is the list of
  // arguments passed from JS and should be an empty list.
  void HandleShowSyncLoginUI(const base::ListValue* args);
#endif

  // Records actions in SyncPromo.NTPPromo histogram.
  void RecordInHistogram(NTPSignInPromoBuckets type);

  // Internal helper method
  void UpdateLogin();

  // Watches this web UI's profile for info changes (e.g. authenticated username
  // changes).
  std::unique_ptr<ProfileInfoWatcher> profile_info_watcher_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherLoginHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_LOGIN_HANDLER_H_
