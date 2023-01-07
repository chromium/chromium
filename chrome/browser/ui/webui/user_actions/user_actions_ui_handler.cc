// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/user_actions/user_actions_ui_handler.h"

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

UserActionsUIHandler::UserActionsUIHandler()
    : action_callback_(base::BindRepeating(&UserActionsUIHandler::OnUserAction,
                                           base::Unretained(this))) {}

UserActionsUIHandler::~UserActionsUIHandler() {
  base::RemoveActionCallback(action_callback_);
}

void UserActionsUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "pageLoaded", base::BindRepeating(&UserActionsUIHandler::HandlePageLoaded,
                                        base::Unretained(this)));
}

void UserActionsUIHandler::HandlePageLoaded(const base::Value::List& args) {
  AllowJavascript();
}

void UserActionsUIHandler::OnJavascriptAllowed() {
  base::AddActionCallback(action_callback_);
}

void UserActionsUIHandler::OnJavascriptDisallowed() {
  base::RemoveActionCallback(action_callback_);
}

void UserActionsUIHandler::OnUserAction(const std::string& action,
                                        base::TimeTicks action_time) {
  if (!IsJavascriptAllowed())
    return;
  base::Value user_action_name(action);

  FireWebUIListener("user-action", user_action_name);
}
