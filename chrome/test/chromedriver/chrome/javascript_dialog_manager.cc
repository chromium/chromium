// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

JavaScriptDialogManager::JavaScriptDialogManager(
    DevToolsClient* client,
    const BrowserInfo* browser_info)
    : client_(client) {
  client_->AddListener(this);
}

JavaScriptDialogManager::~JavaScriptDialogManager() {}

bool JavaScriptDialogManager::IsDialogOpen() const {
  return !unhandled_dialog_queue_.empty();
}

Status JavaScriptDialogManager::GetDialogMessage(std::string* message) {
  if (!IsDialogOpen())
    return Status(kNoSuchAlert);

  *message = unhandled_dialog_queue_.front();
  return Status(kOk);
}

Status JavaScriptDialogManager::GetTypeOfDialog(std::string* type) {
  if (!IsDialogOpen())
    return Status(kNoSuchAlert);

  *type = dialog_type_queue_.front();
  return Status(kOk);
}

Status JavaScriptDialogManager::HandleDialog(bool accept,
                                             const std::string* text) {
  if (!IsDialogOpen())
    return Status(kNoSuchAlert);

  base::DictionaryValue params;
  params.GetDict().Set("accept", accept);
  if (text)
    params.GetDict().Set("promptText", *text);
  else
    params.GetDict().Set("promptText", prompt_text_);
  Status status = client_->SendCommand("Page.handleJavaScriptDialog", params);
  if (status.IsError()) {
    // Retry once to work around
    // https://bugs.chromium.org/p/chromedriver/issues/detail?id=1500
    status = client_->SendCommand("Page.handleJavaScriptDialog", params);
    if (status.IsError())
      return status;
  }
  // Remove a dialog from the queue. Need to check the queue is not empty here,
  // because it could have been cleared during waiting for the command
  // response.
  if (unhandled_dialog_queue_.size())
    unhandled_dialog_queue_.pop_front();

  if (dialog_type_queue_.size())
    dialog_type_queue_.pop_front();

  return Status(kOk);
}

Status JavaScriptDialogManager::OnConnected(DevToolsClient* client) {
  unhandled_dialog_queue_.clear();
  dialog_type_queue_.clear();
  base::DictionaryValue params;
  return client_->SendCommand("Page.enable", params);
}

Status JavaScriptDialogManager::OnEvent(DevToolsClient* client,
                                        const std::string& method,
                                        const base::DictionaryValue& params) {
  if (method == "Page.javascriptDialogOpening") {
    std::string message;
    if (!params.GetString("message", &message))
      return Status(kUnknownError, "dialog event missing or invalid 'message'");

    unhandled_dialog_queue_.push_back(message);

    std::string type;
    if (!params.GetString("type", &type))
      return Status(kUnknownError, "dialog has invalid 'type'");

    dialog_type_queue_.push_back(type);

    if (!params.GetString("defaultPrompt", &prompt_text_))
      return Status(kUnknownError,
                    "dialog event missing or invalid 'defaultPrompt'");
  } else if (method == "Page.javascriptDialogClosed") {
    // Inspector only sends this event when all dialogs have been closed.
    // Clear the unhandled queue in case the user closed a dialog manually.
    unhandled_dialog_queue_.clear();
    dialog_type_queue_.clear();
  }
  return Status(kOk);
}
