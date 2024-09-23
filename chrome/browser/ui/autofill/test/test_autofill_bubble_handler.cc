// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"

#include "chrome/browser/ui/autofill/add_new_address_bubble_controller.h"
#include "chrome/browser/ui/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_controller.h"
#include "chrome/browser/ui/autofill/payments/save_iban_ui.h"
#include "chrome/browser/ui/autofill/save_address_bubble_controller.h"
#include "chrome/browser/ui/autofill/update_address_bubble_controller.h"

namespace autofill {

TestAutofillBubbleHandler::TestAutofillBubbleHandler() = default;

TestAutofillBubbleHandler::~TestAutofillBubbleHandler() = default;

AutofillBubbleBase* TestAutofillBubbleHandler::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller,
    bool is_user_gesture) {
  if (!save_card_bubble_view_)
    save_card_bubble_view_ = std::make_unique<TestAutofillBubble>();
  return save_card_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowIbanBubble(
    content::WebContents* web_contents,
    IbanBubbleController* controller,
    bool is_user_gesture,
    IbanBubbleType bubble_type) {
  if (!iban_bubble_view_) {
    iban_bubble_view_ = std::make_unique<TestAutofillBubble>();
  }
  return iban_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowLocalCardMigrationBubble(
    content::WebContents* web_contents,
    LocalCardMigrationBubbleController* controller,
    bool is_user_gesture) {
  if (!local_card_migration_bubble_view_) {
    local_card_migration_bubble_view_ = std::make_unique<TestAutofillBubble>();
  }
  return local_card_migration_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowOfferNotificationBubble(
    content::WebContents* web_contents,
    OfferNotificationBubbleController* controller,
    bool is_user_gesture) {
  if (!offer_notification_bubble_view_)
    offer_notification_bubble_view_ = std::make_unique<TestAutofillBubble>();
  return offer_notification_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowSaveAddressProfileBubble(
    content::WebContents* contents,
    std::unique_ptr<SaveAddressBubbleController> controller,
    bool is_user_gesture) {
  if (!save_address_profile_bubble_view_)
    save_address_profile_bubble_view_ = std::make_unique<TestAutofillBubble>();
  return save_address_profile_bubble_view_.get();
}

AutofillBubbleBase*
TestAutofillBubbleHandler::ShowSaveAutofillPredictionImprovementsBubble(
    content::WebContents* contents,
    SaveAutofillPredictionImprovementsController* controller) {
  if (!save_autofill_prediction_improvements_bubble_view_) {
    save_autofill_prediction_improvements_bubble_view_ =
        std::make_unique<TestAutofillBubble>();
  }
  return save_autofill_prediction_improvements_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowUpdateAddressProfileBubble(
    content::WebContents* contents,
    std::unique_ptr<UpdateAddressBubbleController> controller,
    bool is_user_gesture) {
  if (!update_address_profile_bubble_view_) {
    update_address_profile_bubble_view_ =
        std::make_unique<TestAutofillBubble>();
  }
  return update_address_profile_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowAddNewAddressProfileBubble(
    content::WebContents* contents,
    std::unique_ptr<AddNewAddressBubbleController> controller,
    bool is_user_gesture) {
  if (!add_new_address_profile_bubble_view_) {
    add_new_address_profile_bubble_view_ =
        std::make_unique<TestAutofillBubble>();
  }
  return add_new_address_profile_bubble_view_.get();
}

AutofillBubbleBase*
TestAutofillBubbleHandler::ShowVirtualCardManualFallbackBubble(
    content::WebContents* web_contents,
    VirtualCardManualFallbackBubbleController* controller,
    bool is_user_gesture) {
  if (!virtual_card_manual_fallback_bubble_view_) {
    virtual_card_manual_fallback_bubble_view_ =
        std::make_unique<TestAutofillBubble>();
  }
  return virtual_card_manual_fallback_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowVirtualCardEnrollBubble(
    content::WebContents* web_contents,
    VirtualCardEnrollBubbleController* controller,
    bool is_user_gesture) {
  if (!virtual_card_enroll_bubble_view_) {
    virtual_card_enroll_bubble_view_ = std::make_unique<TestAutofillBubble>();
  }
  return virtual_card_enroll_bubble_view_.get();
}

AutofillBubbleBase*
TestAutofillBubbleHandler::ShowVirtualCardEnrollConfirmationBubble(
    content::WebContents* web_contents,
    VirtualCardEnrollBubbleController* controller) {
  if (!virtual_card_enroll_confirmation_bubble_view_) {
    virtual_card_enroll_confirmation_bubble_view_ =
        std::make_unique<TestAutofillBubble>();
  }
  return virtual_card_enroll_confirmation_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowMandatoryReauthBubble(
    content::WebContents* web_contents,
    MandatoryReauthBubbleController* controller,
    bool is_user_gesture,
    MandatoryReauthBubbleType bubble_type) {
  if (!mandatory_reauth_bubble_view_) {
    mandatory_reauth_bubble_view_ = std::make_unique<TestAutofillBubble>();
  }
  return mandatory_reauth_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowSaveCardConfirmationBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller) {
  if (!save_card_confirmation_bubble_view_) {
    save_card_confirmation_bubble_view_ =
        std::make_unique<TestAutofillBubble>();
  }
  return save_card_confirmation_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowSaveIbanConfirmationBubble(
    content::WebContents* web_contents,
    IbanBubbleController* controller) {
  if (!save_iban_confirmation_bubble_view_) {
    save_iban_confirmation_bubble_view_ =
        std::make_unique<TestAutofillBubble>();
  }
  return save_iban_confirmation_bubble_view_.get();
}

}  // namespace autofill
