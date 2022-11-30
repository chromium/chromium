// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_USER_ACTIONS_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_USER_ACTIONS_UI_HANDLER_H_

#include "base/metrics/user_metrics.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class TimeTicks;
}  // namespace base

// UI Handler for chrome://user-actions/
// It listens to user action notifications and passes those notifications
// into the Javascript to update the page.
class UserActionsUIHandler : public content::WebUIMessageHandler {
 public:
  UserActionsUIHandler();

  UserActionsUIHandler(const UserActionsUIHandler&) = delete;
  UserActionsUIHandler& operator=(const UserActionsUIHandler&) = delete;

  ~UserActionsUIHandler() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandlePageLoaded(const base::Value::List& args);
  void OnUserAction(const std::string& action, base::TimeTicks action_time);

  base::ActionCallback action_callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_USER_ACTIONS_USER_ACTIONS_UI_HANDLER_H_
