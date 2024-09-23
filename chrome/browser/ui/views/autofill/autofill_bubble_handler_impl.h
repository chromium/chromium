// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/autofill/core/browser/ui/payments/save_payment_method_and_virtual_card_enroll_confirmation_ui_params.h"

class Browser;
class PageActionIconView;
class ToolbarButtonProvider;

namespace content {
class WebContents;
}

namespace views {
class View;
}

namespace autofill {
class AutofillBubbleBase;
class LocalCardMigrationBubbleController;
class SaveCardBubbleController;
class IbanBubbleController;
enum class IbanBubbleType;

class AutofillBubbleHandlerImpl : public AutofillBubbleHandler {
 public:
  AutofillBubbleHandlerImpl(Browser* browser,
                            ToolbarButtonProvider* toolbar_button_provider);

  AutofillBubbleHandlerImpl(const AutofillBubbleHandlerImpl&) = delete;
  AutofillBubbleHandlerImpl& operator=(const AutofillBubbleHandlerImpl&) =
      delete;

  ~AutofillBubbleHandlerImpl() override;

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
      content::WebContents* web_contents,
      std::unique_ptr<SaveAddressBubbleController> controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowUpdateAddressProfileBubble(
      content::WebContents* web_contents,
      std::unique_ptr<UpdateAddressBubbleController> controller,
      bool is_user_gesture) override;
  AutofillBubbleBase* ShowAddNewAddressProfileBubble(
      content::WebContents* web_contents,
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
  // Show the save card and virtual card enrollment confirmation bubble.
  AutofillBubbleBase* ShowSaveCardAndVirtualCardEnrollConfirmationBubble(
      views::View* anchor_view,
      content::WebContents* web_contents,
      base::OnceCallback<void(PaymentsBubbleClosedReason)>
          controller_hide_callback,
      PageActionIconView* icon_view,
      SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params);

  raw_ptr<Browser> browser_ = nullptr;

  raw_ptr<ToolbarButtonProvider> toolbar_button_provider_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_BUBBLE_HANDLER_IMPL_H_
