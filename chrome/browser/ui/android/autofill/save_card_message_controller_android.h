// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_MESSAGE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_MESSAGE_CONTROLLER_ANDROID_H_

#include "chrome/browser/ui/android/autofill/save_card_controller_metrics_android.h"
#include "chrome/browser/ui/android/autofill/save_card_message_confirm_controller.h"
#include "chrome/browser/ui/android/autofill/save_card_message_confirm_delegate.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

class CreditCard;

// Message controller to show a save card message on Android, which
// is destined to replace the save card infobar UI.
class SaveCardMessageControllerAndroid : public SaveCardMessageConfirmDelegate {
 public:
  SaveCardMessageControllerAndroid();
  ~SaveCardMessageControllerAndroid();
  SaveCardMessageControllerAndroid(const SaveCardMessageControllerAndroid&) =
      delete;
  SaveCardMessageControllerAndroid& operator=(
      const SaveCardMessageControllerAndroid&) = delete;

  // Show the ui to offer credit card save to the user.
  // If |upload_save_card_prompt_callback| is not null, the controller
  // assumes the card should be uploaded to cloud. In this case,
  // |local_save_card_prompt_callback| must be null.
  // Similarly, if |local_save_card_prompt_callback| is not null and
  // |upload_save_card_prompt_callback| is null, the controller assumes
  // card should be saved locally.
  void Show(content::WebContents* web_contents,
            AutofillClient::SaveCreditCardOptions options,
            const CreditCard& card,
            const LegalMessageLines& legal_message_lines,
            std::u16string inferred_name,
            AutofillClient::UploadSaveCardPromptCallback
                upload_save_card_prompt_callback,
            AutofillClient::LocalSaveCardPromptCallback
                local_save_card_prompt_callback);

 private:
  friend class SaveCardMessageControllerAndroidTest;

  void HandleMessageDismiss(messages::DismissReason dismiss_reason);
  void HandleMessageAction();
  void DismissMessage();

  void ConfirmDate(const int month, const int year);
  void ConfirmDate();
  void ConfirmName(const std::u16string& inferred_cardholder_name);

  // SaveCardMessageConfirmDelegate
  void OnNameConfirmed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& name) override;
  void OnDateConfirmed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& month,
      const base::android::JavaParamRef<jstring>& year) override;
  void PromptDismissed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) override;
  void OnLegalMessageLinkClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url) override;

  void MaybeShowDialog();

  bool IsGooglePayBrandingEnabled() const;

  // Runs the appropriate local or upload save callback with the given
  // |user_decision|, using the |user_provided_details|. If
  // |user_provided_details| is empty then the current Card values will be used.
  // The cardholder name and expiration date portions of
  // |user_provided_details| are handled separately, so if either of them are
  // empty the current Card values will be used in their place.
  void OnPromptCompleted(
      AutofillClient::SaveCardOfferUserDecision user_decision,
      AutofillClient::UserProvidedCardDetails user_provided_details);

  // Did the user ever explicitly accept or dismiss this message?
  bool HadUserInteraction();

  // Whether the action is an upload or a local save.
  bool is_upload_;

  // The callback to run once the user makes a decision with respect to the
  // credit card upload offer-to-save prompt (if |upload_| is true).
  AutofillClient::UploadSaveCardPromptCallback
      upload_save_card_prompt_callback_;

  // The callback to run once the user makes a decision with respect to the
  // local credit card offer-to-save prompt (if |upload_| is false).
  AutofillClient::LocalSaveCardPromptCallback local_save_card_prompt_callback_;

  // Weak reference to read & write |kAutofillAcceptSaveCreditCardPromptState|.
  PrefService* pref_service_;

  // If the cardholder name is missing, request the name from the user before
  // saving the card. If the expiration date is missing, request the missing
  // data from the user before saving the card.
  AutofillClient::SaveCreditCardOptions options_;

  content::WebContents* web_contents_;

  // Delegate of a toast style popup showing in the top of the screen.
  std::unique_ptr<messages::MessageWrapper> message_;

  std::unique_ptr<SaveCardMessageConfirmController>
      save_card_message_confirm_controller_;

  std::u16string inferred_name_;

  // Whether we need to request users to fill in more info
  bool promo_continue_;
  int expiration_date_year_;
  int expiration_date_month_;

  bool is_name_confirmed_for_testing_ = false;
  bool is_date_confirmed_for_testing_ = false;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_MESSAGE_CONTROLLER_ANDROID_H_
