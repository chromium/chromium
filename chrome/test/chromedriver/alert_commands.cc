// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/alert_commands.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "chrome/test/chromedriver/session.h"

Status ExecuteAlertCommand(const AlertCommand& alert_command,
                           Session* session,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->HandleReceivedEvents();
  if (status.IsError())
    return status;

  status = web_view->WaitForPendingNavigations(
      session->GetCurrentFrameId(), Timeout(session->page_load_timeout), true);
  if (status.IsError() && status.code() != kUnexpectedAlertOpen)
    return status;

  return alert_command.Run(session, web_view, params, value);
}

Status ExecuteGetAlert(Session* session,
                       WebView* web_view,
                       const base::Value::Dict& params,
                       std::unique_ptr<base::Value>* value) {
  *value = std::make_unique<base::Value>(web_view->IsDialogOpen());
  return Status(kOk);
}

Status ExecuteGetAlertText(Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value) {
  std::string message;
  Status status = web_view->GetDialogMessage(message);
  if (status.IsError())
    return status;
  *value = std::make_unique<base::Value>(message);
  return Status(kOk);
}

Status ExecuteSetAlertText(Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value) {
  const std::string* text = params.FindString("text");
  if (!text)
    return Status(kInvalidArgument, "missing or invalid 'text'");

  if (!web_view->IsDialogOpen()) {
    return Status(kNoSuchAlert);
  }

  std::string type;
  Status status = web_view->GetTypeOfDialog(type);
  if (status.IsError())
    return status;

  if (type == "prompt")
    session->prompt_text = std::make_optional<std::string>(*text);
  else if (type == "alert" || type == "confirm")
    return Status(kElementNotInteractable,
                  "User dialog does not have a text box input field.");
  else
    return Status(kUnsupportedOperation,
                  "Text can only be sent to window.prompt dialogs.");
  return Status(kOk);
}

Status ExecuteAcceptAlert(Session* session,
                          WebView* web_view,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value) {
  Status status = web_view->HandleDialog(true, session->prompt_text);
  session->prompt_text.reset();
  return status;
}

Status ExecuteDismissAlert(Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value) {
  Status status = web_view->HandleDialog(false, session->prompt_text);
  session->prompt_text.reset();
  return status;
}
