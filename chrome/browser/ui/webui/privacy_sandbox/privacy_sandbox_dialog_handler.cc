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
    base::OnceClosure show_dialog_callback,
    base::OnceClosure open_settings_callback,
    PrivacySandboxService::PromptType prompt_type)
    : close_callback_(std::move(close_callback)),
      resize_callback_(std::move(resize_callback)),
      show_dialog_callback_(std::move(show_dialog_callback)),
      open_settings_callback_(std::move(open_settings_callback)),
      prompt_type_(prompt_type) {
  DCHECK(close_callback_);
  DCHECK(resize_callback_);
  DCHECK(show_dialog_callback_);
  DCHECK(open_settings_callback_);
}

PrivacySandboxDialogHandler::~PrivacySandboxDialogHandler() {
  DisallowJavascript();
}

void PrivacySandboxDialogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "promptActionOccurred",
      base::BindRepeating(
          &PrivacySandboxDialogHandler::HandlePromptActionOccurred,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resizeDialog",
      base::BindRepeating(&PrivacySandboxDialogHandler::HandleResizeDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showDialog",
      base::BindRepeating(&PrivacySandboxDialogHandler::HandleShowDialog,
                          base::Unretained(this)));
}

void PrivacySandboxDialogHandler::OnJavascriptAllowed() {
  // Initialize the service reference here because `web_ui()` was already
  // initialized. `web_ui()` isn't ready in the constructor.
  privacy_sandbox_service_ =
      PrivacySandboxServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  DCHECK(privacy_sandbox_service_);
}

void PrivacySandboxDialogHandler::OnJavascriptDisallowed() {
  if (did_user_make_decision_)
    return;

  // If user hasn't made a decision, notify the service.
  if (prompt_type_ == PrivacySandboxService::PromptType::kConsent) {
    NotifyServiceAboutPromptAction(
        PrivacySandboxService::PromptAction::kConsentClosedNoDecision);
  } else {
    NotifyServiceAboutPromptAction(
        PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction);
  }
}

void PrivacySandboxDialogHandler::HandlePromptActionOccurred(
    const base::Value::List& args) {
  if (!IsJavascriptAllowed())
    return;

  CHECK_EQ(1U, args.size());
  auto action =
      static_cast<PrivacySandboxService::PromptAction>(args[0].GetInt());

  if (action == PrivacySandboxService::PromptAction::kNoticeOpenSettings)
    std::move(open_settings_callback_).Run();

  switch (action) {
    case PrivacySandboxService::PromptAction::kNoticeAcknowledge:
    case PrivacySandboxService::PromptAction::kNoticeDismiss:
    case PrivacySandboxService::PromptAction::kNoticeOpenSettings:
    case PrivacySandboxService::PromptAction::kConsentAccepted:
    case PrivacySandboxService::PromptAction::kConsentDeclined: {
      did_user_make_decision_ = true;
      DisallowJavascript();
      std::move(close_callback_).Run();
      break;
    }
    default:
      break;
  }

  NotifyServiceAboutPromptAction(action);
}

void PrivacySandboxDialogHandler::HandleResizeDialog(
    const base::Value::List& args) {
  AllowJavascript();

  const base::Value& callback_id = args[0];
  int height = args[1].GetInt();
  DCHECK(resize_callback_);
  std::move(resize_callback_).Run(height);

  ResolveJavascriptCallback(callback_id, base::Value());
}

void PrivacySandboxDialogHandler::HandleShowDialog(
    const base::Value::List& args) {
  AllowJavascript();

  // Notify the service that the DOM was loaded and the dialog was shown to
  // user.
  if (prompt_type_ == PrivacySandboxService::PromptType::kConsent) {
    NotifyServiceAboutPromptAction(
        PrivacySandboxService::PromptAction::kConsentShown);
  } else {
    NotifyServiceAboutPromptAction(
        PrivacySandboxService::PromptAction::kNoticeShown);
  }

  DCHECK(show_dialog_callback_);
  std::move(show_dialog_callback_).Run();
}

void PrivacySandboxDialogHandler::NotifyServiceAboutPromptAction(
    PrivacySandboxService::PromptAction action) {
  DCHECK(privacy_sandbox_service_);
  privacy_sandbox_service_->PromptActionOccurred(action);
}
