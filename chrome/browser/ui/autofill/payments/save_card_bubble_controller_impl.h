// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_

#include <memory>
#include <optional>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/autofill/payments/save_payment_icon_controller.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace autofill {

enum class BubbleType;

// Implementation of per-tab class to control the local/server save credit card
// bubble, the local/server save CVC bubble, and Omnibox icon.
// TODO(crbug.com/40934022): Refactor SaveCardBubbleControllerImpl to split the
// states into different classes.
class SaveCardBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public SaveCardBubbleController,
      public SavePaymentIconController,
      public content::WebContentsUserData<SaveCardBubbleControllerImpl> {
 public:
  SaveCardBubbleControllerImpl(const SaveCardBubbleControllerImpl&) = delete;
  SaveCardBubbleControllerImpl& operator=(const SaveCardBubbleControllerImpl&) =
      delete;
  ~SaveCardBubbleControllerImpl() override;

  // Time in sec to wait before auto-closing confirmation bubble.
  static constexpr base::TimeDelta kAutoCloseConfirmationBubbleWaitSec =
      base::Seconds(3);

  // Sets up the controller and is responsible for offering both local card save
  // and local CVC save. The offer-to-save CVC bubble saves CVC for an existing
  // local card.
  // |save_card_prompt_callback| will be invoked once the user makes a decision
  // with respect to the offer-to-save prompt.
  // If |options.show_bubble| is true, pops up the offer-to-save bubble;
  // otherwise, only the omnibox icon is displayed.
  // If |options.card_save_type| has value `CardSaveType::kCardSaveOnly`, the
  // offer-to-save card bubble is shown. If the value is
  // `CardSaveType::kCardSaveWithCvc`, the offer-to-save card bubble is shown,
  // and the users are informed that the CVC will also be stored. If the type is
  // `CardSaveType::kCvcSaveOnly`, the offer-to-save CVC bubble is shown.
  // TODO(crbug.com/40937065) refactor: pass Iban by value since all they then
  // immediately move it into a member.
  virtual void OfferLocalSave(
      const CreditCard& card,
      payments::PaymentsAutofillClient::SaveCreditCardOptions options,
      payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
          save_card_prompt_callback);

  // Sets up the controller and is responsible for offering both card save and
  // CVC save to Google Payments. The offer-to-save CVC bubble uploads CVC for
  // an existing server card.
  // |save_card_prompt_callback| will be invoked once the user makes a decision
  // with respect to the offer-to-save prompt.
  // The contents of |legal_message_lines| will be displayed in the bubble.
  // If |options.should_request_name_from_user| is true, a textfield confirming
  // the cardholder name will appear in the bubble.
  // If |options.should_request_expiration_date_from_user| is true, a pair of
  // dropdowns for entering the expiration date will appear in the bubble.
  // If |options.show_prompt| is true, pops up the offer-to-save bubble;
  // Otherwise, only the omnibox icon is displayed.
  // If |options.card_save_type| has value `CardSaveType::kCardSaveOnly`, the
  // offer-to-save card bubble is shown. If the value is
  // `CardSaveType::kCardSaveWithCvc`, the offer-to-save card bubble is shown,
  // and the users are informed that the CVC will also be stored. If the type is
  // `CardSaveType::kCvcSaveOnly`, the offer-to-save CVC bubble is shown.
  void OfferUploadSave(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      payments::PaymentsAutofillClient::SaveCreditCardOptions options,
      payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
          save_card_prompt_callback);

  // Exists for testing purposes only. (Otherwise shown through ReshowBubble())
  // Sets up the controller for the Manage Cards view. This displays the card
  // just saved and links the user to manage their other cards.
  void ShowBubbleForManageCardsForTesting(const CreditCard& card);

  void ReshowBubble(bool is_user_gesture);

  // Shows upload result to users. `card_saved` indicates if the card is
  // successfully saved. `on_confirmation_closed_callback` will be invoked
  // once confirmation bubble is closed. Posts a delayed task to auto-close the
  // confirmation bubble if user doesn't close the bubble before
  // `kAutoCloseConfirmationBubbleWaitSec`.
  virtual void ShowConfirmationBubbleView(
      bool card_saved,
      std::optional<
          payments::PaymentsAutofillClient::OnConfirmationClosedCallback>
          on_confirmation_closed_callback);

  // SaveCardBubbleController:
  std::u16string GetWindowTitle() const override;
  std::u16string GetExplanatoryMessage() const override;
  std::u16string GetAcceptButtonText() const override;
  std::u16string GetDeclineButtonText() const override;
  AccountInfo GetAccountInfo() override;
  Profile* GetProfile() const override;
  const CreditCard& GetCard() const override;
  base::OnceCallback<void(PaymentsBubbleClosedReason)>
  GetOnBubbleClosedCallback() override;
  const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams&
  GetConfirmationUiParams() const override;
  bool ShouldRequestNameFromUser() const override;
  bool ShouldRequestExpirationDateFromUser() const override;
  ui::ImageModel GetCreditCardImage() const override;

  void OnSaveButton(
      const payments::PaymentsAutofillClient::UserProvidedCardDetails&
          user_provided_card_details) override;
  void OnLegalMessageLinkClicked(const GURL& url) override;
  void OnManageCardsClicked() override;
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;
  const LegalMessageLines& GetLegalMessageLines() const override;
  bool IsUploadSave() const override;
  BubbleType GetBubbleType() const override;
  bool IsPaymentsSyncTransportEnabledWithoutSyncFeature() const override;
  void HideSaveCardBubble() override;

  // SavePaymentIconController:
  std::u16string GetSavePaymentIconTooltipText() const override;
  bool ShouldShowSavingPaymentAnimation() const override;
  bool ShouldShowPaymentSavedLabelAnimation() const override;
  void OnAnimationEnded() override;
  bool IsIconVisible() const override;
  AutofillBubbleBase* GetPaymentBubbleView() const override;
  int GetSaveSuccessAnimationStringId() const override;

  static base::AutoReset<bool> IgnoreWindowActivationForTesting();

 protected:
  explicit SaveCardBubbleControllerImpl(content::WebContents* web_contents);

  // Opens the Payments settings page.
  virtual void ShowPaymentsSettingsPage();

  // AutofillBubbleControllerBase::
  void OnVisibilityChanged(content::Visibility visibility) override;
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<SaveCardBubbleControllerImpl>;
  friend class SaveCardBubbleControllerImplTest;
  friend class SaveCardBubbleViewsFullFormBrowserTest;

  // Displays both the offer-to-save bubble and is associated omnibox icon.
  void ShowBubble();

  // Displays the omnibox icon without popping up the offer-to-save bubble.
  void ShowIconOnly();

  void UpdateSaveCardIcon();

  void OpenUrl(const GURL& url);

  // Returns whether the web contents related to the controller is active.
  bool IsWebContentsActive();

  // Should outlive this object.
  raw_ptr<PersonalDataManager> personal_data_manager_;

  // Should outlive this object.
  raw_ptr<syncer::SyncService> sync_service_;

  // Is true only if the [Card saved] label animation should be shown.
  bool should_show_card_saved_label_animation_ = false;

  // The type of bubble that is either currently being shown or would
  // be shown when the save card icon is clicked.
  BubbleType current_bubble_type_ = BubbleType::INACTIVE;

  // Callback to run once the user makes a decision with respect to the credit
  // card upload offer-to-save prompt or the CVC upload offer-to-save prompt
  // for existing server cards.
  // For credit card upload offer-to-save prompt, will return the cardholder
  // name provided/confirmed by the user if it was requested. Will also return
  // the expiration month and year provided by the user if the expiration date
  // was requested.
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
      upload_save_card_prompt_callback_;

  // Callback to run once the user makes a decision with respect to the local
  // credit card offer-to-save prompt or the local CVC offer-to-save prompt.
  payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
      local_save_card_prompt_callback_;

  // Callback to run after save card confirmation bubble is closed.
  std::optional<payments::PaymentsAutofillClient::OnConfirmationClosedCallback>
      on_confirmation_closed_callback_;

  // Governs whether the upload or local save version of the UI should be shown.
  bool is_upload_save_ = false;

  // Whether the bubble view show is prompted by a user gesture. This is used
  // when showing the bubble view to set the display reason for
  // LocationBarBubbleDelegateView::ShowForReason() which will determine whether
  // the bubble view is initially active or inactive when created.
  bool is_triggered_by_user_gesture_ = false;

  // Whether the bubble view show is a re-show of the view.
  bool is_reshow_ = false;

  // Whether a url was opened from the bubble view.
  bool was_url_opened_ = false;

  // `options_.should_request_name_from_user`, whether the upload save version
  // of the UI should surface a textfield requesting the cardholder name.
  // `options_.should_request_expiration_date_from_user`, Whether the upload
  // save version of the UI should surface a pair of dropdowns requesting the
  // expiration date.
  // `options_.show_prompt` Whether the offer-to-save bubble should be shown or
  // not. If true, behaves normally. If false, the omnibox icon will be
  // displayed when offering credit card save, but the bubble itself will not
  // pop up.
  // `options_.card_save_type` If the type is `CardSaveType::kCardSaveOnly`, the
  // offer-to-save card bubble is shown. If the type is
  // `CardSaveType::kCardSaveWithCvc`, the offer-to-save card bubble is shown,
  // and the users are informed that the CVC will also be stored. If the type is
  // `CardSaveType::kCvcSaveOnly`, the offer-to-save CVC bubble is shown.
  payments::PaymentsAutofillClient::SaveCreditCardOptions options_;

  // Contains the details of the card that will be saved if the user accepts.
  CreditCard card_;

  // If no legal message should be shown then this variable is an empty vector.
  LegalMessageLines legal_message_lines_;

  // UI parameters needed to display the save card confirmation view.
  std::optional<SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams>
      confirmation_ui_params_;

  // Timer that controls auto closure of confirmation bubble. Should be
  // stopped once the bubble is closed.
  base::OneShotTimer auto_close_confirmation_timer_;

  // Weak pointer factory for this save card bubble controller.
  base::WeakPtrFactory<SaveCardBubbleControllerImpl> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
