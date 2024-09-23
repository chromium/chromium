// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TEST_TEST_AUTOFILL_BUBBLE_HANDLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_TEST_TEST_AUTOFILL_BUBBLE_HANDLER_H_

#include <memory>

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"

namespace autofill {

enum class IbanBubbleType;
enum class MandatoryReauthBubbleType;

class TestAutofillBubble final : public AutofillBubbleBase {
  void Hide() override {}
};

class TestAutofillBubbleHandler : public AutofillBubbleHandler {
 public:
  TestAutofillBubbleHandler();

  TestAutofillBubbleHandler(const TestAutofillBubbleHandler&) = delete;
  TestAutofillBubbleHandler& operator=(const TestAutofillBubbleHandler&) =
      delete;

  ~TestAutofillBubbleHandler() override;

  // AutofillBubbleHandler:
  AutofillBubbleBase* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowIbanBubble(content::WebContents* web_contents,
                                     IbanBubbleController* controller,
                                     bool is_user_gesture,
                                     IbanBubbleType bubble_type) override;
  AutofillBubbleBase* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowOfferNotificationBubble(
      content::WebContents* contents,
      OfferNotificationBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowSaveAutofillPredictionImprovementsBubble(
      content::WebContents* web_contents,
      SaveAutofillPredictionImprovementsController* controller) override;
  AutofillBubbleBase* ShowSaveAddressProfileBubble(
      content::WebContents* contents,
      std::unique_ptr<SaveAddressBubbleController> controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowUpdateAddressProfileBubble(
      content::WebContents* contents,
      std::unique_ptr<UpdateAddressBubbleController> controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowAddNewAddressProfileBubble(
      content::WebContents* contents,
      std::unique_ptr<AddNewAddressBubbleController> controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowVirtualCardManualFallbackBubble(
      content::WebContents* web_contents,
      VirtualCardManualFallbackBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowVirtualCardEnrollBubble(
      content::WebContents* web_contents,
      VirtualCardEnrollBubbleController* controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowVirtualCardEnrollConfirmationBubble(
      content::WebContents* web_contents,
      VirtualCardEnrollBubbleController* controller) override;
  AutofillBubbleBase* ShowMandatoryReauthBubble(
      content::WebContents* web_contents,
      MandatoryReauthBubbleController* controller,
      bool is_user_gesture,
      MandatoryReauthBubbleType bubble_type) override;
  AutofillBubbleBase* ShowSaveCardConfirmationBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller) override;
  AutofillBubbleBase* ShowSaveIbanConfirmationBubble(
      content::WebContents* web_contents,
      IbanBubbleController* controller) override;

 private:
  std::unique_ptr<TestAutofillBubble> local_card_migration_bubble_view_;
  std::unique_ptr<TestAutofillBubble> offer_notification_bubble_view_;
  std::unique_ptr<TestAutofillBubble> save_card_bubble_view_;
  std::unique_ptr<TestAutofillBubble> iban_bubble_view_;
  std::unique_ptr<TestAutofillBubble> save_address_profile_bubble_view_;
  std::unique_ptr<TestAutofillBubble> update_address_profile_bubble_view_;
  std::unique_ptr<TestAutofillBubble>
      save_autofill_prediction_improvements_bubble_view_;
  std::unique_ptr<TestAutofillBubble> add_new_address_profile_bubble_view_;
  std::unique_ptr<TestAutofillBubble> edit_address_profile_bubble_view_;
  std::unique_ptr<TestAutofillBubble> virtual_card_manual_fallback_bubble_view_;
  std::unique_ptr<TestAutofillBubble> virtual_card_enroll_bubble_view_;
  std::unique_ptr<TestAutofillBubble>
      virtual_card_enroll_confirmation_bubble_view_;
  std::unique_ptr<TestAutofillBubble> mandatory_reauth_bubble_view_;
  std::unique_ptr<TestAutofillBubble> save_card_confirmation_bubble_view_;
  std::unique_ptr<TestAutofillBubble> save_iban_confirmation_bubble_view_;

  int save_card_confirmation_bubble_shown_count_ = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_TEST_TEST_AUTOFILL_BUBBLE_HANDLER_H_
