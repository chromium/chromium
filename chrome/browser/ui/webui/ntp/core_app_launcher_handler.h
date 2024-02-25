// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_CORE_APP_LAUNCHER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_CORE_APP_LAUNCHER_HANDLER_H_

#include <string>

#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

class CoreAppLauncherHandler : public content::WebUIMessageHandler {
 public:
  CoreAppLauncherHandler();

  CoreAppLauncherHandler(const CoreAppLauncherHandler&) = delete;
  CoreAppLauncherHandler& operator=(const CoreAppLauncherHandler&) = delete;

  ~CoreAppLauncherHandler() override;

 private:
  // Callback for the "recordAppLaunchByUrl" message. Takes an escaped URL and
  // a launch source(integer), and if the URL represents an app, records the
  // action for UMA.
  void HandleRecordAppLaunchByUrl(const base::Value::List& args);

  // Records an app launch in the corresponding |bucket| of the app launch
  // histogram if the |escaped_url| corresponds to an installed app.
  void RecordAppLaunchByUrl(Profile* profile,
                            std::string url,
                            extension_misc::AppLaunchBucket bucket);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_CORE_APP_LAUNCHER_HANDLER_H_
