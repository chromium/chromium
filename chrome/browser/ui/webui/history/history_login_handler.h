// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_LOGIN_HANDLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "content/public/browser/web_ui_message_handler.h"

class HistorySignInStateWatcher;

// The handler for login-related messages from chrome://history.
class HistoryLoginHandler : public content::WebUIMessageHandler {
 public:
  explicit HistoryLoginHandler(
      base::RepeatingClosure signin_state_changed_callback);

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

  // Handler for the "recordSigninPendingOffered" message. No args.
  void HandleRecordSigninPendingOffered(const base::Value::List& args);

  // Called by |history_sign_in_state_watcher_| when the signin state changes
  void SigninStateChanged();

  // Watches for changes to the history-related sign-in state.
  std::unique_ptr<HistorySignInStateWatcher> history_sign_in_state_watcher_;

  base::RepeatingClosure signin_state_changed_callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_LOGIN_HANDLER_H_
