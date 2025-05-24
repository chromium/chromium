// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_handler.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace {

using enum PrivacySandboxService::AdsDialogCallbackNoArgsEvents;
using enum PrivacySandboxService::PromptAction;

bool IsConsent(PrivacySandboxService::PromptType prompt_type) {
  return prompt_type == PrivacySandboxService::PromptType::kM1Consent;
}

bool IsRestrictedNotice(PrivacySandboxService::PromptType prompt_type) {
  return prompt_type == PrivacySandboxService::PromptType::kM1NoticeRestricted;
}

void NotifyServiceAboutPromptAction(
    PrivacySandboxService::PromptAction action,
    PrivacySandboxService* privacy_sandbox_service) {
  CHECK(privacy_sandbox_service);
  privacy_sandbox_service->PromptActionOccurred(
      action, PrivacySandboxService::SurfaceType::kDesktop);
}

}  // namespace

PrivacySandboxDialogHandler::PrivacySandboxDialogHandler(
    base::RepeatingCallback<void(
        PrivacySandboxService::AdsDialogCallbackNoArgsEvents)> dialog_callback,
    base::OnceCallback<void(int)> resize_callback,
    PrivacySandboxService::PromptType prompt_type)
    : dialog_callback_(std::move(dialog_callback)),
      resize_callback_(std::move(resize_callback)),
      prompt_type_(prompt_type) {
  DCHECK(dialog_callback_);
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
  web_ui()->RegisterMessageCallback(
      "recordPrivacyPolicyLoadTime",
      base::BindRepeating(
          &PrivacySandboxDialogHandler::HandleRecordPrivacyPolicyLoadTime,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "shouldShowAdTopicsContentParity",
      base::BindRepeating(
          &PrivacySandboxDialogHandler::HandleShouldShowAdTopicsContentParity,
          base::Unretained(this)));
}

void PrivacySandboxDialogHandler::HandleRecordPrivacyPolicyLoadTime(
    const base::Value::List& args) {
  if (!IsJavascriptAllowed()) {
    return;
  }

  auto privacy_policy_page_load_duration = args[0].GetDouble();
  // This just means the page was already preloaded so there is no load time.
  if (privacy_policy_page_load_duration < 0) {
    privacy_policy_page_load_duration = 0;
  }

  base::UmaHistogramTimes(
      "PrivacySandbox.PrivacyPolicy.LoadingTime",
      base::Milliseconds(privacy_policy_page_load_duration));
}

void PrivacySandboxDialogHandler::HandleShouldShowAdTopicsContentParity(
    const base::Value::List& args) {
  AllowJavascript();
  ResolveJavascriptCallback(
      args[0], base::FeatureList::IsEnabled(
                   privacy_sandbox::kPrivacySandboxAdTopicsContentParity));
}

void PrivacySandboxDialogHandler::OnJavascriptAllowed() {
  // Initialize the service reference here because `web_ui()` was already
  // initialized. `web_ui()` isn't ready in the constructor.
  privacy_sandbox_service_ =
      PrivacySandboxServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  DCHECK(privacy_sandbox_service_);
}

void PrivacySandboxDialogHandler::OnJavascriptDisallowed() {
  if (did_user_make_decision_) {
    return;
  }

  // If user hasn't made a decision, notify the service.
  if (IsConsent(prompt_type_)) {
    NotifyServiceAboutPromptAction(kConsentClosedNoDecision,
                                   privacy_sandbox_service_);
  } else if (IsRestrictedNotice(prompt_type_)) {
    NotifyServiceAboutPromptAction(kRestrictedNoticeClosedNoInteraction,
                                   privacy_sandbox_service_);
  } else {
    NotifyServiceAboutPromptAction(kNoticeClosedNoInteraction,
                                   privacy_sandbox_service_);
  }
}

void PrivacySandboxDialogHandler::HandlePromptActionOccurred(
    const base::Value::List& args) {
  if (!IsJavascriptAllowed()) {
    return;
  }

  CHECK_EQ(1U, args.size());
  auto action =
      static_cast<PrivacySandboxService::PromptAction>(args[0].GetInt());

  switch (action) {
    case kNoticeAcknowledge:
    case kRestrictedNoticeAcknowledge:
    case kNoticeDismiss: {
      final_decision_count_++;
      CloseDialog();
      break;
    }
    case kNoticeOpenSettings: {
      dialog_callback_.Run(kOpenAdsPrivacySettings);
      final_decision_count_++;
      CloseDialog();
      break;
    }
    case kRestrictedNoticeOpenSettings: {
      dialog_callback_.Run(kOpenMeasurementSettings);
      final_decision_count_++;
      CloseDialog();
      break;
    }
    case kConsentAccepted:
    case kConsentDeclined: {
      did_user_make_decision_ = true;
      break;
    }
    default:
      break;
  }

  NotifyServiceAboutPromptAction(action, privacy_sandbox_service_);
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

  CHECK(dialog_callback_);
  did_dialog_show_ = true;
  dialog_callback_.Run(kShowDialog);
}

void PrivacySandboxDialogHandler::CloseDialog() {
  did_user_make_decision_ = true;
  DisallowJavascript();
  if (!dialog_callback_) {
    if (!did_dialog_show_) {
      base::UmaHistogramEnumeration(
          "PrivacySandbox.Notice.CloseDialogCallbackState",
          PrivacySandboxDialogCallbackState::kCallbackUnknownBeforeShown);
    } else if (final_decision_count_ > 1) {
      base::UmaHistogramEnumeration(
          "PrivacySandbox.Notice.CloseDialogCallbackState",
          PrivacySandboxDialogCallbackState::kMultiActionCallbackDNE);
    } else {
      base::UmaHistogramEnumeration(
          "PrivacySandbox.Notice.CloseDialogCallbackState",
          PrivacySandboxDialogCallbackState::kSingleActionCallbackDNE);
    }
  }
  dialog_callback_.Run(kCloseDialog);
}

void PrivacySandboxDialogHandler::SetDialogCallbackForTesting(
    const base::RepeatingCallback<
        void(PrivacySandboxService::AdsDialogCallbackNoArgsEvents)>& callback) {
  dialog_callback_ = std::move(callback);
}
