// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_card_message_controller_android.h"

#include "base/strings/stringprintf.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_utils_mobile.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/grit/components_scaled_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

namespace autofill {

SaveCardMessageControllerAndroid::SaveCardMessageControllerAndroid() {}

SaveCardMessageControllerAndroid::~SaveCardMessageControllerAndroid() {
  DismissMessage();
  web_contents_ = nullptr;
}

void SaveCardMessageControllerAndroid::Show(
    content::WebContents* web_contents,
    AutofillClient::SaveCreditCardOptions options,
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    std::u16string inferred_name,
    AutofillClient::UploadSaveCardPromptCallback
        upload_save_card_prompt_callback,
    AutofillClient::LocalSaveCardPromptCallback
        local_save_card_prompt_callback) {
  DCHECK(!upload_save_card_prompt_callback != !local_save_card_prompt_callback);
  if (message_) {
    // Dismiss the currently-shown message so that the new one can be displayed.
    DismissMessage();
  }
  web_contents_ = web_contents;
  pref_service_ = Profile::FromBrowserContext(web_contents->GetBrowserContext())
                      ->GetPrefs();
  options_ = options;
  inferred_name_ = inferred_name;

  upload_save_card_prompt_callback_ =
      std::move(upload_save_card_prompt_callback);
  local_save_card_prompt_callback_ = std::move(local_save_card_prompt_callback);
  is_upload_ = !local_save_card_prompt_callback_;

  save_card_message_confirm_controller_ =
      std::make_unique<SaveCardMessageConfirmController>(this, web_contents);

  if (is_upload_ && !legal_message_lines.empty()) {
    save_card_message_confirm_controller_->SetLegalMessageLine(
        legal_message_lines.front());
  }

  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::SAVE_CARD,
      base::BindOnce(&SaveCardMessageControllerAndroid::HandleMessageAction,
                     base::Unretained(this)),
      base::BindOnce(&SaveCardMessageControllerAndroid::HandleMessageDismiss,
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

  // Client won't request both name and expiration date at the same time.
  promo_continue_ = options.should_request_name_from_user ||
                    options.should_request_expiration_date_from_user;
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      promo_continue_ ? IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE
                      : IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));
  if (is_upload_ && !promo_continue_) {
    expiration_date_year_ = card.expiration_year();
    expiration_date_month_ = card.expiration_month();
  }

  // Web_contents scope: show message along with the tab. Auto-dismissed when
  // tab is closed or time is up.
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);

  LogAutofillCreditCardMessageMetrics(MessageMetrics::kShown, is_upload_,
                                      options_);
}

bool SaveCardMessageControllerAndroid::IsGooglePayBrandingEnabled() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return is_upload_;
#else
  return false;
#endif
}

void SaveCardMessageControllerAndroid::HandleMessageAction() {
  MaybeShowDialog();
}

void SaveCardMessageControllerAndroid::HandleMessageDismiss(
    messages::DismissReason dismiss_reason) {
  if (dismiss_reason != messages::DismissReason::PRIMARY_ACTION &&
      !HadUserInteraction()) {
    // Gesture: users explicitly swipe the UI to dismiss the message
    bool gesture_dismiss = dismiss_reason == messages::DismissReason::GESTURE;
    OnPromptCompleted(
        gesture_dismiss ? AutofillClient::DECLINED : AutofillClient::IGNORED,
        /*user_provided_details=*/{});
  }
  message_.reset();
}

void SaveCardMessageControllerAndroid::DismissMessage() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

void SaveCardMessageControllerAndroid::OnPromptCompleted(
    AutofillClient::SaveCardOfferUserDecision user_decision,
    AutofillClient::UserProvidedCardDetails user_provided_details) {
  MessageMetrics message_state;
  switch (user_decision) {
    case AutofillClient::ACCEPTED:
      message_state = MessageMetrics::kAccepted;
      break;
    case AutofillClient::DECLINED:
      message_state = MessageMetrics::kDenied;
      break;
    case AutofillClient::IGNORED:
      message_state = MessageMetrics::kIgnored;
      break;
  }
  LogAutofillCreditCardMessageMetrics(message_state, is_upload_, options_);
  UpdateAutofillAcceptSaveCreditCardPromptState(
      pref_service_, message_state == MessageMetrics::kAccepted);
  if (is_upload_) {
    std::move(upload_save_card_prompt_callback_)
        .Run(user_decision, user_provided_details);
  } else {
    std::move(local_save_card_prompt_callback_).Run(user_decision);
  }
}

void SaveCardMessageControllerAndroid::MaybeShowDialog() {
  if (is_upload_ && !promo_continue_) {
    // If we already know all the info, confirm the date to show
    // other info such as legal terms,
    // and then run callback after user confirms
    ConfirmDate(expiration_date_month_, expiration_date_year_);
  } else if (options_.should_request_name_from_user) {
    ConfirmName(inferred_name_);
  } else if (options_.should_request_expiration_date_from_user) {
    ConfirmDate();
  } else {
    OnPromptCompleted(AutofillClient::ACCEPTED, {});
  }
}

bool SaveCardMessageControllerAndroid::HadUserInteraction() {
  // Callbacks have been executed only when user has interacted with ui.
  return !upload_save_card_prompt_callback_ &&
         !local_save_card_prompt_callback_;
}

// --- Confirm date ---

void SaveCardMessageControllerAndroid::ConfirmDate() {
  save_card_message_confirm_controller_->ConfirmDate(
      message_->GetDescription()  // card label
  );
  is_date_confirmed_for_testing_ = true;
}

void SaveCardMessageControllerAndroid::ConfirmDate(const int month,
                                                   const int year) {
  save_card_message_confirm_controller_->ConfirmDate(
      base::StringPrintf("%d", month), base::StringPrintf("%d", year),
      message_->GetDescription()  // card label
  );
  is_date_confirmed_for_testing_ = true;
}

void SaveCardMessageControllerAndroid::OnDateConfirmed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& month,
    const base::android::JavaParamRef<jstring>& year) {
  OnPromptCompleted(
      AutofillClient::ACCEPTED,
      {std::u16string(), base::android::ConvertJavaStringToUTF16(month),
       base::android::ConvertJavaStringToUTF16(year)});
}

// --- Confirm name ---

void SaveCardMessageControllerAndroid::ConfirmName(
    const std::u16string& inferred_cardholder_name) {
  save_card_message_confirm_controller_->ConfirmName(
      inferred_cardholder_name,
      message_->GetDescription()  // card label
  );
  is_name_confirmed_for_testing_ = true;
}

void SaveCardMessageControllerAndroid::OnNameConfirmed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& name) {
  OnPromptCompleted(AutofillClient::ACCEPTED,
                    {base::android::ConvertJavaStringToUTF16(name),
                     std::u16string(), std::u16string()});
}

// --- Dismiss callbacks ---

void SaveCardMessageControllerAndroid::PromptDismissed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!HadUserInteraction()) {
    OnPromptCompleted(AutofillClient::DECLINED,
                      /*user_provided_details=*/{});
  }
  save_card_message_confirm_controller_.reset();
}

void SaveCardMessageControllerAndroid::OnLegalMessageLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& url) {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(base::android::ConvertJavaStringToUTF16(url)), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false));
}

}  // namespace autofill
