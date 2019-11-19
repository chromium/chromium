// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace autofill {

enum class BubbleType;

// Implementation of per-tab class to control the save credit card bubble and
// Omnibox icon.
class SaveCardBubbleControllerImpl
    : public SaveCardBubbleController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<SaveCardBubbleControllerImpl> {
 public:
  // An observer class used by browsertests that gets notified whenever
  // particular actions occur.
  class ObserverForTest {
   public:
    virtual void OnBubbleShown() = 0;
    virtual void OnBubbleClosed() = 0;
  };

  ~SaveCardBubbleControllerImpl() override;

  // Sets up the controller and offers to save the |card| locally.
  // |save_card_prompt_callback| will be invoked once the user makes a decision
  // with respect to the offer-to-save prompt. If
  // |options.show_bubble| is true, pops up the offer-to-save
  // bubble; otherwise, only the omnibox icon is displayed.
  // If |options.has_non_focusable_field| is true, the save is triggered by a
  // form that has non_focusable fields.
  // If |options.from_dynamic_change_form| is true, the save is triggered by a
  // dynamic change form.
  void OfferLocalSave(
      const CreditCard& card,
      AutofillClient::SaveCreditCardOptions options,
      AutofillClient::LocalSaveCardPromptCallback save_card_prompt_callback);

  // Sets up the controller and offers to upload the |card| to Google Payments.
  // |save_card_prompt_callback| will be invoked once the user makes a decision
  // with respect to the offer-to-save prompt. The contents of
  // |legal_message_lines| will be displayed in the bubble. A textfield
  // confirming the cardholder name will appear in the bubble if
  // |options.should_request_name_from_user| is true. A pair of
  // dropdowns for entering the expiration date will appear in the bubble if
  // |options.should_request_expiration_date_from_user| is
  // true. If |options.show_prompt| is true, pops up the
  // offer-to-save bubble; otherwise, only the omnibox icon is displayed.
  // If |options.has_non_focusable_field| is true, the save is triggered by a
  // form that has non-focusable fields.
  // If |options.from_dynamic_change_form| is true, the save is triggered by a
  // dynamic change form.
  void OfferUploadSave(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      AutofillClient::SaveCreditCardOptions options,
      AutofillClient::UploadSaveCardPromptCallback save_card_prompt_callback);

  // Sets up the controller for the sign in promo and shows the bubble.
  // This bubble is only shown after a local save is accepted and if
  // |ShouldShowSignInPromo()| returns true.
  void MaybeShowBubbleForSignInPromo();

  // Exists for testing purposes only. (Otherwise shown through ReshowBubble())
  // Sets up the controller for the Manage Cards view. This displays the card
  // just saved and links the user to manage their other cards.
  void ShowBubbleForManageCardsForTesting(const CreditCard& card);

  // Update the icon when card is successfully saved. This will dismiss the icon
  // and trigger a highlight animation of the avatar button.
  void UpdateIconForSaveCardSuccess();

  // Updates the save card icon when credit card upload failed. This will only
  // update the icon image and stop icon from animating. The actual bubble will
  // be shown when users click on the icon.
  void UpdateIconForSaveCardFailure();

  // For testing. Sets up the controller for showing the
  // save card failure bubble.
  void ShowBubbleForSaveCardFailureForTesting();

  void HideBubble();
  // TODO(crbug.com/932818): Maybe move sign in promo completely out of this
  // class, and merge with password sign in promo.
  void HideBubbleForSignInPromo();

  void ReshowBubble();

  // SaveCardBubbleController:
  base::string16 GetWindowTitle() const override;
  base::string16 GetExplanatoryMessage() const override;
  base::string16 GetAcceptButtonText() const override;
  base::string16 GetDeclineButtonText() const override;
  base::string16 GetSaveCardIconTooltipText() const override;
  const AccountInfo& GetAccountInfo() const override;
  Profile* GetProfile() const override;
  const CreditCard& GetCard() const override;
  SaveCardBubbleView* GetSaveCardBubbleView() const override;
  bool ShouldRequestNameFromUser() const override;
  bool ShouldRequestExpirationDateFromUser() const override;

  // Returns true only if at least one of the following cases is true:
  // 1) The user is signed out.
  // 2) The user is signed in through DICe, but did not turn on syncing.
  // Consequently returns false in the following cases:
  // 1) The user has paused syncing (Auth Error).
  // 2) The user is not required to be syncing in order to upload cards
  //    to the server -- this should change.
  // TODO(crbug.com/864702): Don't show promo if user is a butter user.
  bool ShouldShowSignInPromo() const override;
  bool ShouldShowSavingCardAnimation() const override;
  bool ShouldShowCardSavedLabelAnimation() const override;
  bool ShouldShowSaveFailureBadge() const override;
  void OnSyncPromoAccepted(const AccountInfo& account,
                           signin_metrics::AccessPoint access_point,
                           bool is_default_promo_account) override;
  void OnSaveButton(const AutofillClient::UserProvidedCardDetails&
                        user_provided_card_details) override;
  void OnCancelButton() override;
  void OnLegalMessageLinkClicked(const GURL& url) override;
  void OnManageCardsClicked() override;
  void OnBubbleClosed() override;
  void OnAnimationEnded() override;
  const LegalMessageLines& GetLegalMessageLines() const override;
  bool IsIconVisible() const override;
  bool IsUploadSave() const override;
  BubbleType GetBubbleType() const override;
  AutofillSyncSigninState GetSyncState() const override;

 protected:
  explicit SaveCardBubbleControllerImpl(content::WebContents* web_contents);

  // Opens the Payments settings page.
  virtual void ShowPaymentsSettingsPage();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // Gets the security level of the page.
  virtual security_state::SecurityLevel GetSecurityLevel() const;

 private:
  friend class content::WebContentsUserData<SaveCardBubbleControllerImpl>;
  friend class SaveCardBubbleViewsFullFormBrowserTest;

  void FetchAccountInfo();

  // Displays both the offer-to-save bubble and is associated omnibox icon.
  void ShowBubble();

  // Displays the omnibox icon without popping up the offer-to-save bubble.
  void ShowIconOnly();

  void UpdateSaveCardIcon();

  void OpenUrl(const GURL& url);

  // For testing.
  void SetEventObserverForTesting(ObserverForTest* observer) {
    observer_for_testing_ = observer;
  }

  // Should outlive this object.
  PersonalDataManager* personal_data_manager_;

  // Is true only if the [Card saved] label animation should be shown.
  bool should_show_card_saved_label_animation_ = false;

  // Weak reference. Will be nullptr if no bubble is currently shown.
  SaveCardBubbleView* save_card_bubble_view_ = nullptr;

  // The type of bubble that is either currently being shown or would
  // be shown when the save card icon is clicked.
  BubbleType current_bubble_type_ = BubbleType::INACTIVE;

  // Weak reference to read & write |kAutofillAcceptSaveCreditCardPromptState|.
  PrefService* pref_service_;

  // Callback to run once the user makes a decision with respect to the credit
  // card upload offer-to-save prompt. Will return the cardholder name
  // provided/confirmed by the user if it was requested. Will also return the
  // expiration month and year provided by the user if the expiration date was
  // requested. If both callbacks are null then no bubble is available to show
  // and the icon is not visible.
  AutofillClient::UploadSaveCardPromptCallback
      upload_save_card_prompt_callback_;

  // Callback to run once the user makes a decision with respect to the local
  // credit card offer-to-save prompt. If both callbacks return true for
  // .is_null() then no bubble is available to show and the icon is not visible.
  AutofillClient::LocalSaveCardPromptCallback local_save_card_prompt_callback_;

  // Governs whether the upload or local save version of the UI should be shown.
  bool is_upload_save_ = false;

  // Whether ReshowBubble() has been called since ShowBubbleFor*() was called.
  bool is_reshow_ = false;

  // |options_.should_request_name_from_user|, whether the upload save version
  // of the UI should surface a textfield requesting the cardholder name.
  // |options_.should_request_expiration_date_from_user|, Whether the upload
  // save version of the UI should surface a pair of dropdowns requesting the
  // expiration date. |options_.show_prompt| Whether the offer-to-save bubble
  // should be shown or not. If true, behaves normally. If false, the omnibox
  // icon will be displayed when offering credit card save, but the bubble
  // itself will not pop up.
  AutofillClient::SaveCreditCardOptions options_;

  // The account info of the signed-in user.
  AccountInfo account_info_;

  // Contains the details of the card that will be saved if the user accepts.
  CreditCard card_;

  // If no legal message should be shown then this variable is an empty vector.
  LegalMessageLines legal_message_lines_;

  // The time at which the bubble was shown. If it has been visible for less
  // time than some reasonable limit, don't close the bubble upon navigation.
  base::Time bubble_shown_timestamp_;

  // The security level for the current context.
  security_state::SecurityLevel security_level_;

  // Observer for when a bubble is created. Initialized only during tests.
  ObserverForTest* observer_for_testing_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
