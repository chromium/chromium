// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_handler.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"

PrivacySandboxDialogHandler::PrivacySandboxDialogHandler(
    base::OnceClosure close_callback,
    base::OnceCallback<void(int)> resize_callback,
    base::OnceClosure open_settings_callback)
    : close_callback_(std::move(close_callback)),
      resize_callback_(std::move(resize_callback)),
      open_settings_callback_(std::move(open_settings_callback)) {}

PrivacySandboxDialogHandler::~PrivacySandboxDialogHandler() {
  DisallowJavascript();
}

void PrivacySandboxDialogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "dialogActionOccurred",
      base::BindRepeating(
          &PrivacySandboxDialogHandler::HandleDialogActionOccurred,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resizeDialog",
      base::BindRepeating(&PrivacySandboxDialogHandler::HandleResizeDialog,
                          base::Unretained(this)));
}

void PrivacySandboxDialogHandler::HandleDialogActionOccurred(
    base::Value::ConstListView args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  auto action =
      static_cast<PrivacySandboxService::DialogAction>(args[0].GetInt());

  if (action == PrivacySandboxService::DialogAction::kNoticeOpenSettings) {
    DCHECK(open_settings_callback_);
    std::move(open_settings_callback_).Run();
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

  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  DCHECK(privacy_sandbox_service);
  privacy_sandbox_service->DialogActionOccurred(action);
}

void PrivacySandboxDialogHandler::HandleResizeDialog(
    base::Value::ConstListView args) {
  int height = args[0].GetInt();
  DCHECK(resize_callback_);
  std::move(resize_callback_).Run(height);
}
