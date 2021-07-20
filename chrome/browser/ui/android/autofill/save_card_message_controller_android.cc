// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_card_message_controller_android.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_utils_mobile.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/grit/components_scaled_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

SaveCardMessageControllerAndroid::SaveCardMessageControllerAndroid() {}

SaveCardMessageControllerAndroid::~SaveCardMessageControllerAndroid() {
  DismissInternal();
}

void SaveCardMessageControllerAndroid::Show(
    content::WebContents* web_contents,
    AutofillClient::SaveCreditCardOptions options,
    const CreditCard& card,
    AutofillClient::UploadSaveCardPromptCallback
        upload_save_card_prompt_callback,
    AutofillClient::LocalSaveCardPromptCallback
        local_save_card_prompt_callback) {
  DCHECK(!upload_save_card_prompt_callback != !local_save_card_prompt_callback);
  if (message_) {
    // Dismiss the currently-shown message so that the new one can be displayed.
    DismissInternal();
  }
  web_contents_ = web_contents;
  pref_service_ = Profile::FromBrowserContext(web_contents->GetBrowserContext())
                      ->GetPrefs();
  options_ = options;

  upload_save_card_prompt_callback_ =
      std::move(upload_save_card_prompt_callback);
  local_save_card_prompt_callback_ = std::move(local_save_card_prompt_callback);
  is_upload_ = !local_save_card_prompt_callback_;

  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::SAVE_CARD,
      base::BindOnce(&SaveCardMessageControllerAndroid::HandleAction,
                     base::Unretained(this)),
      base::BindOnce(&SaveCardMessageControllerAndroid::HandleDismiss,
                     base::Unretained(this)));
  message_->SetTitle(l10n_util::GetStringUTF16(
      IsGooglePayBrandingEnabled()
          ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V4
          : is_upload_ ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3
                       : IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL));

  message_->SetDescription(card.CardIdentifierStringForAutofillDisplay());

  message_->SetIconResourceId(ResourceMapper::MapToJavaDrawableId(
      GetSaveCardIconId(IsGooglePayBrandingEnabled())));

  if (IsGooglePayBrandingEnabled()) {
    // Do not tint image; otherwise, the image will lose its original color and
    // be filled with a tint color.
    message_->DisableIconTint();
  }

  bool prompt_continue = options.should_request_name_from_user ||
                         options.should_request_expiration_date_from_user;
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      prompt_continue ? IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE
                      : IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));

  // Web_contents scope: show message along with the tab. Auto-dismissed when
  // tab is closed or time is up.
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);
}

bool SaveCardMessageControllerAndroid::IsGooglePayBrandingEnabled() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return is_upload_;
#else
  return false;
#endif
}

void SaveCardMessageControllerAndroid::HandleAction() {
  RunSaveCardPromptCallback(AutofillClient::ACCEPTED,
                            /*user_provided_details=*/{});
}

void SaveCardMessageControllerAndroid::HandleDismiss(
    messages::DismissReason dismiss_reason) {
  if (!HadUserInteraction()) {
    RunSaveCardPromptCallback(AutofillClient::DECLINED,
                              /*user_provided_details=*/{});
  }
  message_.reset();
  web_contents_ = nullptr;
}

void SaveCardMessageControllerAndroid::DismissInternal() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), web_contents_, messages::DismissReason::UNKNOWN);
  }
}

void SaveCardMessageControllerAndroid::RunSaveCardPromptCallback(
    AutofillClient::SaveCardOfferUserDecision user_decision,
    AutofillClient::UserProvidedCardDetails user_provided_details) {
  UpdateAutofillAcceptSaveCreditCardPromptState(
      pref_service_, user_decision == AutofillClient::ACCEPTED);
  if (is_upload_) {
    std::move(upload_save_card_prompt_callback_)
        .Run(user_decision, user_provided_details);
  } else {
    std::move(local_save_card_prompt_callback_).Run(user_decision);
  }
}

bool SaveCardMessageControllerAndroid::HadUserInteraction() {
  // Callbacks have been executed only when user has interacted with ui.
  return !upload_save_card_prompt_callback_ &&
         !local_save_card_prompt_callback_;
}
}  // namespace autofill
