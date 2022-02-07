// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_handler.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"

PrivacySandboxDialogHandler::PrivacySandboxDialogHandler(
    base::OnceClosure close_callback,
    Browser* browser)
    : close_callback_(std::move(close_callback)), browser_(browser) {}

PrivacySandboxDialogHandler::~PrivacySandboxDialogHandler() {
  DisallowJavascript();
}

void PrivacySandboxDialogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "dialogActionOccurred",
      base::BindRepeating(
          &PrivacySandboxDialogHandler::HandleDialogActionOccurred,
          base::Unretained(this)));
}

void PrivacySandboxDialogHandler::HandleDialogActionOccurred(
    base::Value::ConstListView args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  auto action =
      static_cast<PrivacySandboxService::DialogAction>(args[0].GetInt());

  // TODO(crbug.com/1286276): Handle all other actions.
  if (action == PrivacySandboxService::DialogAction::kNoticeOpenSettings) {
    DCHECK(browser_);
    chrome::ShowPrivacySandboxSettings(browser_);
  }

  switch (action) {
    case PrivacySandboxService::DialogAction::kNoticeAcknowledge:
    case PrivacySandboxService::DialogAction::kNoticeOpenSettings:
    case PrivacySandboxService::DialogAction::kConsentAccepted:
    case PrivacySandboxService::DialogAction::kConsentDeclined:
      if (close_callback_)
        std::move(close_callback_).Run();
      break;
    default:
      break;
  }

  LogDialogAction(action);
}

void PrivacySandboxDialogHandler::LogDialogAction(
    PrivacySandboxService::DialogAction action) {
  // TODO(crbug.com/1286276):Add metrics.
}
