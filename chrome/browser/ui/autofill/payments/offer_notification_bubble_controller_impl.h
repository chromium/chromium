// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace autofill {

struct OfferNotificationOptions;

// Implementation of per-tab class to control the offer notification bubble and
// Omnibox icon.
class OfferNotificationBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public OfferNotificationBubbleController,
      public content::WebContentsUserData<
          OfferNotificationBubbleControllerImpl> {
 public:
  // An observer class used by browsertests that gets notified whenever
  // particular actions occur.
  class ObserverForTest {
   public:
    virtual ~ObserverForTest() = default;
    virtual void OnBubbleShown() {}
  };

  ~OfferNotificationBubbleControllerImpl() override;
  OfferNotificationBubbleControllerImpl(
      const OfferNotificationBubbleControllerImpl&) = delete;
  OfferNotificationBubbleControllerImpl& operator=(
      const OfferNotificationBubbleControllerImpl&) = delete;

  // OfferBubbleController:
  std::u16string GetWindowTitle() const override;
  std::u16string GetOkButtonLabel() const override;
  std::u16string GetPromoCodeButtonTooltip() const override;
  AutofillBubbleBase* GetOfferNotificationBubbleView() const override;
  const CreditCard* GetLinkedCard() const override;
  const AutofillOfferData* GetOffer() const override;
  bool IsIconVisible() const override;
  void OnBubbleClosed(PaymentsUiClosedReason closed_reason) override;

  // Displays an offer notification for the given `offer` on the current page.
  // The information of the `card`, if present, will be displayed in the bubble
  // for a card-linked offer. `options` contains information on how the offer
  // notification should show.
  void ShowOfferNotificationIfApplicable(
      const AutofillOfferData& offer,
      const CreditCard* card,
      const OfferNotificationOptions& options);

  // Called when user clicks on omnibox icon.
  void ReshowBubble();

  // Removes any visible bubble and the omnibox icon.
  void DismissNotification();

 protected:
  explicit OfferNotificationBubbleControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase:
  void OnVisibilityChanged(content::Visibility visibility) override;
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

  // Returns whether the web content associated with this controller is active.
  virtual bool IsWebContentsActive();

 private:
  friend class content::WebContentsUserData<
      OfferNotificationBubbleControllerImpl>;
  friend class OfferNotificationBubbleControllerImplTest;
  friend class OfferNotificationBubbleViewsTestBase;

  // Hides the bubble if it is visible and resets the bubble shown timestamp.
  // `should_show_icon` decides whether the icon should be visible after the
  // bubble is dismissed.
  void HideBubbleAndClearTimestamp(bool should_show_icon);

  // For testing.
  void SetEventObserverForTesting(ObserverForTest* observer) {
    observer_for_testing_ = observer;
  }

  // This is a helper method for controlling the page action on the new
  // page actions framework, if the migration is enabled.
  // Currently, `AutofillBubbleControllerBase::UpdatePageActionIcon` only
  // updates to the legacy icon.
  // TODO(crbug.com/402820548): Move this to `AutofillBubbleControllerBase`
  // once per-PageAction migration feature flags are added.
  void UpdatePageAction();

  // The timestamp that the bubble has been shown. Used to check if the bubble
  // has been shown for longer than kAutofillBubbleSurviveNavigationTime.
  std::optional<base::Time> bubble_shown_timestamp_;

  // The Autofill offer being displayed as a bubble. Set when the bubble is
  // requested to be shown via ShowOfferNotificationIfApplicable(~).
  AutofillOfferData offer_;

  // Denotes whether the bubble is shown due to user gesture. If this is true,
  // it means the bubble is a reshown bubble.
  bool is_user_gesture_ = false;

  // The related credit card for a card linked offer. This can be nullopt for
  // offer types other than card linked offers.
  std::optional<CreditCard> card_;

  // Denotes whether the promo code label button was clicked yet or not.
  // Determines the appropriate hover tooltip for the button.
  bool promo_code_button_clicked_ = false;

  // Records the current state of the bubble.
  BubbleState bubble_state_ = BubbleState::kHidden;

  raw_ptr<ObserverForTest> observer_for_testing_ = nullptr;

  const raw_ref<tabs::TabInterface> tab_interface_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_IMPL_H_
