// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_handler.h"

PrivacySandboxDialogHandler::PrivacySandboxDialogHandler(
    base::OnceClosure close_callback)
    : close_callback_(std::move(close_callback)) {}

PrivacySandboxDialogHandler::~PrivacySandboxDialogHandler() {
  DisallowJavascript();
}

void PrivacySandboxDialogHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "closeDialog",
      base::BindRepeating(&PrivacySandboxDialogHandler::HandleCloseDialog,
                          base::Unretained(this)));
}

void PrivacySandboxDialogHandler::HandleCloseDialog(
    const base::ListValue* args) {
  if (close_callback_)
    std::move(close_callback_).Run();
}
