// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"

#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

JavaScriptDialogManager::JavaScriptDialogManager(DevToolsClient* client)
    : client_(client) {
  client_->AddListener(this);
}

JavaScriptDialogManager::~JavaScriptDialogManager() {
  client_->RemoveListener(this);
}

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

  base::Value::Dict params;
  params.Set("accept", accept);
  if (text)
    params.Set("promptText", *text);
  else
    params.Set("promptText", prompt_text_);
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
  base::Value::Dict params;
  return client_->SendCommand("Page.enable", params);
}

Status JavaScriptDialogManager::OnEvent(DevToolsClient* client,
                                        const std::string& method,
                                        const base::Value::Dict& params) {
  if (method == "Page.javascriptDialogOpening") {
    const std::string* message = params.FindString("message");
    if (!message)
      return Status(kUnknownError, "dialog event missing or invalid 'message'");

    unhandled_dialog_queue_.push_back(*message);

    const std::string* type = params.FindString("type");
    if (!type)
      return Status(kUnknownError, "dialog has invalid 'type'");

    dialog_type_queue_.push_back(*type);

    const std::string* prompt_text = params.FindString("defaultPrompt");
    if (!prompt_text) {
      return Status(kUnknownError,
                    "dialog event missing or invalid 'defaultPrompt'");
    }
    prompt_text_ = *prompt_text;
  } else if (method == "Page.javascriptDialogClosed") {
    // Inspector only sends this event when all dialogs have been closed.
    // Clear the unhandled queue in case the user closed a dialog manually.
    unhandled_dialog_queue_.clear();
    dialog_type_queue_.clear();
  }
  return Status(kOk);
}
