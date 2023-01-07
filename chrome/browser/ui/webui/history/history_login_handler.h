// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_LOGIN_HANDLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "content/public/browser/web_ui_message_handler.h"

class ProfileInfoWatcher;

// The handler for login-related messages from chrome://history.
class HistoryLoginHandler : public content::WebUIMessageHandler {
 public:
  explicit HistoryLoginHandler(base::RepeatingClosure signin_callback);

  HistoryLoginHandler(const HistoryLoginHandler&) = delete;
  HistoryLoginHandler& operator=(const HistoryLoginHandler&) = delete;

  ~HistoryLoginHandler() override;

  // WebUIMessageHandler implementation.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

 private:
  // Handler for the "otherDevicesInitialized" message. No args.
  void HandleOtherDevicesInitialized(const base::Value::List& args);

  // Handler for the "startTurnOnSyncFlow" message. No args.
  void HandleTurnOnSyncFlow(const base::Value::List& args);

  // Called by |profile_info_watcher_| on desktop if profile info changes.
  void ProfileInfoChanged();

  // Watches this web UI's profile for info changes (e.g. authenticated username
  // changes).
  std::unique_ptr<ProfileInfoWatcher> profile_info_watcher_;

  base::RepeatingClosure signin_callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_LOGIN_HANDLER_H_
