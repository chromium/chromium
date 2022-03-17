// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_handler.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"

namespace {

// Informs the TrustSafetySentimentService, if it exists for |profile|, that a
// Privacy Sandbox 3 interaction for |area| has occurred.
void InformSentimentService(Profile* profile,
                            TrustSafetySentimentService::FeatureArea area) {
  auto* sentiment_service =
      TrustSafetySentimentServiceFactory::GetForProfile(profile);

  if (!sentiment_service)
    return;

  sentiment_service->InteractedWithPrivacySandbox3(area);
}

}  // namespace

PrivacySandboxDialogHandler::PrivacySandboxDialogHandler(
    base::OnceClosure close_callback,
    base::OnceCallback<void(int)> resize_callback,
    base::OnceClosure show_dialog_callback,
    base::OnceClosure open_settings_callback,
    PrivacySandboxService::DialogType dialog_type)
    : close_callback_(std::move(close_callback)),
      resize_callback_(std::move(resize_callback)),
      show_dialog_callback_(std::move(show_dialog_callback)),
      open_settings_callback_(std::move(open_settings_callback)),
      dialog_type_(dialog_type) {}

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
  if (dialog_type_ == PrivacySandboxService::DialogType::kConsent) {
    NotifyServiceAboutDialogAction(
        PrivacySandboxService::DialogAction::kConsentClosedNoDecision);
  } else {
    NotifyServiceAboutDialogAction(
        PrivacySandboxService::DialogAction::kNoticeClosedNoInteraction);
  }
}

void PrivacySandboxDialogHandler::HandleDialogActionOccurred(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  auto action =
      static_cast<PrivacySandboxService::DialogAction>(args[0].GetInt());

  if (action == PrivacySandboxService::DialogAction::kNoticeOpenSettings) {
    DCHECK(open_settings_callback_);
    std::move(open_settings_callback_).Run();
  }

  bool covered_action = true;
  switch (action) {
    case PrivacySandboxService::DialogAction::kNoticeAcknowledge: {
      InformSentimentService(
          Profile::FromWebUI(web_ui()),
          TrustSafetySentimentService::FeatureArea::kPrivacySandbox3NoticeOk);
      break;
    }
    case PrivacySandboxService::DialogAction::kNoticeDismiss: {
      InformSentimentService(Profile::FromWebUI(web_ui()),
                             TrustSafetySentimentService::FeatureArea::
                                 kPrivacySandbox3NoticeDismiss);
      break;
    }
    case PrivacySandboxService::DialogAction::kNoticeOpenSettings: {
      InformSentimentService(Profile::FromWebUI(web_ui()),
                             TrustSafetySentimentService::FeatureArea::
                                 kPrivacySandbox3NoticeSettings);
      break;
    }
    case PrivacySandboxService::DialogAction::kConsentAccepted: {
      InformSentimentService(Profile::FromWebUI(web_ui()),
                             TrustSafetySentimentService::FeatureArea::
                                 kPrivacySandbox3ConsentAccept);
      break;
    }
    case PrivacySandboxService::DialogAction::kConsentDeclined: {
      InformSentimentService(Profile::FromWebUI(web_ui()),
                             TrustSafetySentimentService::FeatureArea::
                                 kPrivacySandbox3ConsentDecline);
      break;
    }
    default:
      covered_action = false;
      break;
  }

  if (covered_action) {
    did_user_make_decision_ = true;
    DCHECK(close_callback_);
    std::move(close_callback_).Run();
  }

  NotifyServiceAboutDialogAction(action);
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
  if (dialog_type_ == PrivacySandboxService::DialogType::kConsent) {
    NotifyServiceAboutDialogAction(
        PrivacySandboxService::DialogAction::kConsentShown);
  } else {
    NotifyServiceAboutDialogAction(
        PrivacySandboxService::DialogAction::kNoticeShown);
  }

  DCHECK(show_dialog_callback_);
  std::move(show_dialog_callback_).Run();
}

void PrivacySandboxDialogHandler::NotifyServiceAboutDialogAction(
    PrivacySandboxService::DialogAction action) {
  DCHECK(privacy_sandbox_service_);
  privacy_sandbox_service_->DialogActionOccurred(action);
}
