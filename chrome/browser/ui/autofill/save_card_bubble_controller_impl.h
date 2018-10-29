// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/autofill/save_card_ui.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/ui/save_card_bubble_controller.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/core/browser/account_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace autofill {

enum class BubbleType;
class StrikeDatabase;

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
    virtual void OnSCBCStrikeChangeComplete() = 0;
  };

  ~SaveCardBubbleControllerImpl() override;

  // Sets up the controller and offers to save the |card| locally.
  // |save_card_callback| will be invoked if and when the Save button is
  // pressed. If |show_bubble| is true, pops up the offer-to-save bubble;
  // otherwise, only the omnibox icon is displayed.
  void OfferLocalSave(const CreditCard& card,
                      bool show_bubble,
                      base::OnceClosure save_card_callback);

  // Sets up the controller and offers to upload the |card| to Google Payments.
  // |save_card_callback| will be invoked if and when the Save button is
  // pressed. The contents of |legal_message| will be displayed in the bubble.
  // A textfield confirming the cardholder name will appear in the bubble if
  // |should_request_name_from_user| is true. If |show_bubble| is true, pops up
  // the offer-to-save bubble; otherwise, only the omnibox icon is displayed.
  void OfferUploadSave(
      const CreditCard& card,
      std::unique_ptr<base::DictionaryValue> legal_message,
      bool should_request_name_from_user,
      bool show_bubble,
      base::OnceCallback<void(const base::string16&)> save_card_callback);

  // Sets up the controller for the sign in promo and shows the bubble.
  // This bubble is only shown after a local save is accepted and if
  // |ShouldShowSignInPromo()| returns true.
  void ShowBubbleForSignInPromo();

  // Exists for testing purposes only. (Otherwise shown through ReshowBubble())
  // Sets up the controller for the Manage Cards view. This displays the card
  // just saved and links the user to manage their other cards.
  void ShowBubbleForManageCardsForTesting(const CreditCard& card);

  void HideBubble();
  void ReshowBubble();

  // Returns true if Omnibox save credit card icon should be visible.
  bool IsIconVisible() const;

  // Returns nullptr if no bubble is currently shown.
  SaveCardBubbleView* save_card_bubble_view() const;

  // SaveCardBubbleController:
  base::string16 GetWindowTitle() const override;
  base::string16 GetExplanatoryMessage() const override;
  const AccountInfo& GetAccountInfo() const override;
  Profile* GetProfile() const override;
  const CreditCard& GetCard() const override;
  bool ShouldRequestNameFromUser() const override;

  // Returns true only if at least one of the following cases is true:
  // 1) The user is signed out.
  // 2) The user is signed in through DICe, but did not turn on syncing.
  // Consequently returns false in the following cases:
  // 1) The user has paused syncing (Auth Error).
  // 2) The user is not required to be syncing in order to upload cards
  //    to the server -- this should change.
  // TODO(crbug.com/864702): Don't show promo if user is a butter user.
  bool ShouldShowSignInPromo() const override;
  bool CanAnimate() const override;
  void OnSyncPromoAccepted(const AccountInfo& account,
                           signin_metrics::AccessPoint access_point,
                           bool is_default_promo_account) override;
  void OnSaveButton(
      const base::string16& cardholder_name = base::string16()) override;
  void OnCancelButton() override;
  void OnLegalMessageLinkClicked(const GURL& url) override;
  void OnManageCardsClicked() override;
  void OnBubbleClosed() override;
  void OnAnimationEnded() override;
  const LegalMessageLines& GetLegalMessageLines() const override;
  bool IsUploadSave() const override;
  BubbleType GetBubbleType() const override;

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
  friend class SaveCardBubbleViewsBrowserTestBase;

  void FetchAccountInfo();

  // Fetches the Autofill StrikeDatabase for the current profile.
  StrikeDatabase* GetStrikeDatabase();

  // Displays both the offer-to-save bubble and is associated omnibox icon.
  void ShowBubble();

  // Displays the omnibox icon without popping up the offer-to-save bubble.
  void ShowIconOnly();

  // Update the visibility and toggled state of the Omnibox save card icon.
  void UpdateIcon();

  // Used for browsertests. Gives the |observer_for_testing_| a notification
  // a strike change has been made.
  void OnStrikeChangeComplete(const int num_strikes);

  void OpenUrl(const GURL& url);

  // For testing.
  void SetEventObserverForTesting(ObserverForTest* observer) {
    observer_for_testing_ = observer;
  }

  // The web_contents associated with this controller.
  content::WebContents* web_contents_;

  // Is true only if the card saved animation can be shown.
  bool can_animate_ = false;

  // Weak reference. Will be nullptr if no bubble is currently shown.
  SaveCardBubbleView* save_card_bubble_view_ = nullptr;

  // The type of bubble that is either currently being shown or would
  // be shown when the save card icon is clicked.
  BubbleType current_bubble_type_ = BubbleType::INACTIVE;

  // Weak reference to read & write |kAutofillAcceptSaveCreditCardPromptState|.
  PrefService* pref_service_;

  // Callback to run if user presses Save button in the upload save bubble. Will
  // return the cardholder name provided/confirmed by the user if it was
  // requested. If both callbacks are null then no bubble is available to show
  // and the icon is not visible.
  base::OnceCallback<void(const base::string16&)> upload_save_card_callback_;

  // Callback to run if user presses Save button in the local save bubble. If
  // both callbacks return true for .is_null() then no bubble is available to
  // show and the icon is not visible.
  base::OnceClosure local_save_card_callback_;

  // Governs whether the upload or local save version of the UI should be shown.
  bool is_upload_save_ = false;

  // Whether ReshowBubble() has been called since ShowBubbleFor*() was called.
  bool is_reshow_ = false;

  // Whether the upload save version of the UI should surface a textfield
  // requesting the cardholder name.
  bool should_request_name_from_user_ = false;

  // Whether the offer-to-save bubble should be shown or not. If true, behaves
  // normally. If false, the omnibox icon will be displayed when offering credit
  // card save, but the bubble itself will not pop up.
  bool show_bubble_ = true;

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

  base::WeakPtrFactory<SaveCardBubbleControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_BUBBLE_CONTROLLER_IMPL_H_
