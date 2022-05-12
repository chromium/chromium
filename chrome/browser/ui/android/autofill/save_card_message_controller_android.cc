// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_card_message_controller_android.h"

#include "base/strings/stringprintf.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/autofill/save_card_controller_metrics_android.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_utils_mobile.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/grit/components_scaled_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/messages/android/messages_feature.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

namespace autofill {

SaveCardMessageControllerAndroid::SaveCardMessageControllerAndroid() {}

SaveCardMessageControllerAndroid::~SaveCardMessageControllerAndroid() {
  DismissMessage();
  // The dialog has shown before but user didn't take any action, like
  // user navigates to ToS page and then closes the previous tab.
  if (is_dialog_shown_ && !HadUserInteraction()) {
    OnPromptCompleted(SaveCreditCardPromptResult::kInteractedAndIgnored,
                      /*user_provided_details=*/{});
  }
}

void SaveCardMessageControllerAndroid::Show(
    content::WebContents* web_contents,
    AutofillClient::SaveCreditCardOptions options,
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    std::u16string inferred_name,
    std::u16string cardholder_account,
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
  options_ = options;
  inferred_name_ = inferred_name;
  cardholder_account_ = cardholder_account;

  upload_save_card_prompt_callback_ =
      std::move(upload_save_card_prompt_callback);
  local_save_card_prompt_callback_ = std::move(local_save_card_prompt_callback);
  is_upload_ = !local_save_card_prompt_callback_;

  save_card_message_confirm_controller_ =
      std::make_unique<SaveCardMessageConfirmController>(this, web_contents);

  if (is_upload_) {
    for (const auto& line : legal_message_lines) {
      save_card_message_confirm_controller_->AddLegalMessageLine(line);
    }
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

  card_label_ = card.CardIdentifierStringForAutofillDisplay();
  std::u16string expiry_date =
      card.AbbreviatedExpirationDateForDisplay(/*with_prefix=*/false);
  message_->SetDescription(
      expiry_date.empty() ? card_label_ : card_label_ + u"   " + expiry_date);

  bool use_gpay_icon =
      IsGooglePayBrandingEnabled() && messages::UseGPayIconForSaveCardMessage();

  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(GetSaveCardIconId(use_gpay_icon)));

  if (use_gpay_icon) {
    // Do not tint image; otherwise, the image will lose its original color and
    // be filled with a tint color.
    message_->DisableIconTint();
  }

  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  message_->SetSecondaryButtonMenuText(
      l10n_util::GetStringUTF16(IDS_NO_THANKS));
  message_->SetSecondaryActionCallback(base::BindOnce(
      &SaveCardMessageControllerAndroid::HandleMessageSecondaryButtonClicked,
      base::Unretained(this)));

  // Client won't request both name and expiration date at the same time.
  request_more_info_ = options.should_request_name_from_user ||
                       options.should_request_expiration_date_from_user;

  // Show "continue" when uploading no matter if more info is requested, because
  // legal terms must be displayed in dialogs when uploading.
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      request_more_info_ || is_upload_
          ? (messages::UseFollowupButtonTextForSaveCardMessage()
                 ? IDS_AUTOFILL_MOBILE_SAVE_CARD_TO_CLOUD_PROMPT_SAVE_FOLLOW_UP
                 : IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE)
          : IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT));

  // Web_contents scope: show message along with the tab. Auto-dismissed when
  // tab is closed or time is up.
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);

  LogAutofillCreditCardMessageMetrics(MessageMetrics::kShown, is_upload_,
                                      options_);
}

void SaveCardMessageControllerAndroid::OnWebContentsFocused() {
  // User should not be able to leave page by clicking on
  // links in the legal messages.
  if (reprompt_required_) {
    MaybeShowDialog();
  }
}

// Called when message is dismissed. Message is dismissed when primary action
// is called, when page is closed while message is still on screen, or when
// it is auto dismissed.
void SaveCardMessageControllerAndroid::HandleMessageDismiss(
    messages::DismissReason dismiss_reason) {
  if (dismiss_reason != messages::DismissReason::PRIMARY_ACTION &&
      !HadUserInteraction()) {
    // Gesture: users explicitly swipe the UI to dismiss the message
    bool gesture_dismiss = dismiss_reason == messages::DismissReason::GESTURE;
    OnPromptCompleted(gesture_dismiss ? SaveCreditCardPromptResult::kDenied
                                      : SaveCreditCardPromptResult::kIgnored,
                      /*user_provided_details=*/{});
  }
  // Reset all if we won't show dialogs in the next steps
  if (HadUserInteraction()) {
    ResetInternal();
  }
  message_.reset();
}

void SaveCardMessageControllerAndroid::HandleMessageAction() {
  MaybeShowDialog();
}

void SaveCardMessageControllerAndroid::HandleMessageSecondaryButtonClicked() {
  messages::MessageDispatcherBridge::Get()->DismissMessage(
      message_.get(), messages::DismissReason::SECONDARY_ACTION);
}

void SaveCardMessageControllerAndroid::DismissMessage() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

void SaveCardMessageControllerAndroid::MaybeShowDialog() {
  reprompt_required_ = false;
  if (is_upload_ && !request_more_info_) {
    // If we already know all the info, confirm save card to show other info
    // such as legal terms, and then run callback after user confirms.
    is_dialog_shown_ = true;
    ConfirmSaveCard();
  } else if (options_.should_request_name_from_user) {
    is_dialog_shown_ = true;
    FixName(inferred_name_);
  } else if (options_.should_request_expiration_date_from_user) {
    is_dialog_shown_ = true;
    FixDate();
  } else {
    OnPromptCompleted(SaveCreditCardPromptResult::kAccepted, {});
  }
}

// --- Confirm card save, including date or name if necessary ---

void SaveCardMessageControllerAndroid::FixName(
    const std::u16string& inferred_cardholder_name) {
  save_card_message_confirm_controller_->FixName(
      inferred_cardholder_name, card_label_, cardholder_account_);
  is_name_confirmed_for_testing_ = true;
}

void SaveCardMessageControllerAndroid::FixDate() {
  save_card_message_confirm_controller_->FixDate(card_label_,
                                                 cardholder_account_);
  is_date_confirmed_for_testing_ = true;
}

void SaveCardMessageControllerAndroid::ConfirmSaveCard() {
  save_card_message_confirm_controller_->ConfirmSaveCard(card_label_,
                                                         cardholder_account_);
  is_save_card_confirmed_for_testing_ = true;
}

// --- On card save, cardholder name, or date confirmed ---

void SaveCardMessageControllerAndroid::OnNameConfirmed(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& name) {
  OnPromptCompleted(SaveCreditCardPromptResult::kAccepted,
                    {base::android::ConvertJavaStringToUTF16(name),
                     std::u16string(), std::u16string()});
}

void SaveCardMessageControllerAndroid::OnDateConfirmed(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& month,
    const base::android::JavaParamRef<jstring>& year) {
  OnPromptCompleted(
      SaveCreditCardPromptResult::kAccepted,
      {std::u16string(), base::android::ConvertJavaStringToUTF16(month),
       base::android::ConvertJavaStringToUTF16(year)});
}

void SaveCardMessageControllerAndroid::OnSaveCardConfirmed(JNIEnv* env) {
  OnPromptCompleted(SaveCreditCardPromptResult::kAccepted, {});
}

// --- Dialog Dismissed ---

void SaveCardMessageControllerAndroid::DialogDismissed(JNIEnv* env) {
  if (reprompt_required_) {
    return;
  }
  if (!HadUserInteraction()) {
    OnPromptCompleted(SaveCreditCardPromptResult::kInteractedAndIgnored,
                      /*user_provided_details=*/{});
  }
  ResetInternal();
}

void SaveCardMessageControllerAndroid::OnLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& url) {
  reprompt_required_ = true;
  is_link_clicked_ = true;
  // Temporarily dismiss the dialog and then re-prompt when user returns to
  // the page.
  save_card_message_confirm_controller_->DismissDialog();
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(base::android::ConvertJavaStringToUTF16(url)), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false));
}

bool SaveCardMessageControllerAndroid::IsGooglePayBrandingEnabled() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return is_upload_;
#else
  return false;
#endif
}

void SaveCardMessageControllerAndroid::OnPromptCompleted(
    SaveCreditCardPromptResult save_result,
    AutofillClient::UserProvidedCardDetails user_provided_details) {
  MessageMetrics message_state;
  MessageDialogPromptMetrics dialog_state;
  AutofillClient::SaveCardOfferUserDecision user_decision;
  switch (save_result) {
    case SaveCreditCardPromptResult::kAccepted:
      user_decision = AutofillClient::SaveCardOfferUserDecision::kAccepted;
      message_state = MessageMetrics::kAccepted;
      dialog_state = MessageDialogPromptMetrics::kAccepted;
      break;
    case SaveCreditCardPromptResult::kDenied:
      user_decision = AutofillClient::SaveCardOfferUserDecision::kDeclined;
      message_state = MessageMetrics::kDenied;
      dialog_state = MessageDialogPromptMetrics::kDenied;
      break;
    case SaveCreditCardPromptResult::kIgnored:
      user_decision = AutofillClient::SaveCardOfferUserDecision::kIgnored;
      message_state = MessageMetrics::kIgnored;
      dialog_state = MessageDialogPromptMetrics::kIgnored;
      break;
    case SaveCreditCardPromptResult::kInteractedAndIgnored:
      // kIgnore in following metrics is equivalent to kInteractedAndIgnored and
      // kIgnored of SaveCreditCardPromptResult.
      user_decision = AutofillClient::SaveCardOfferUserDecision::kIgnored;
      message_state = MessageMetrics::kIgnored;
      dialog_state = MessageDialogPromptMetrics::kIgnored;
      break;
  }
  LogAutofillCreditCardMessageMetrics(message_state, is_upload_, options_);
  LogSaveCreditCardPromptResult(save_result, is_upload_, options_);
  if (is_upload_) {
    if (is_dialog_shown_) {
      LogAutofillCreditCardMessageDialogPromptMetrics(dialog_state, options_,
                                                      is_link_clicked_);
    }
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

void SaveCardMessageControllerAndroid::ResetInternal() {
  message_.reset();
  reprompt_required_ = false;
  web_contents_ = nullptr;
  save_card_message_confirm_controller_.reset();
}

}  // namespace autofill
