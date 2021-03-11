// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

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
  AutofillBubbleBase* GetOfferNotificationBubbleView() const override;
  const CreditCard* GetLinkedCard() const override;
  bool IsIconVisible() const override;
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;

  // Displays an offer notification on current page. Populates the value for
  // |origins_to_display_bubble_|, since the bubble and icon are sticky over a
  // given set of origins. For a card linked offer, The information of the
  // |card| will be displayed in the bubble.
  void ShowOfferNotificationIfApplicable(
      const std::vector<GURL>& origins_to_display_bubble,
      const CreditCard* card);

  // Called when user clicks on omnibox icon.
  void ReshowBubble();

 protected:
  explicit OfferNotificationBubbleControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<
      OfferNotificationBubbleControllerImpl>;
  friend class OfferNotificationBubbleViewsTestBase;

  // Returns whether the web content associated with this controller is active.
  bool IsWebContentsActive();

  // For testing.
  void SetEventObserverForTesting(ObserverForTest* observer) {
    observer_for_testing_ = observer;
  }

  // Denotes whether the bubble is shown due to user gesture. If this is true,
  // it means the bubble is a reshown bubble.
  bool is_user_gesture_ = false;

  // The related credit card for a card linked offer. This can be nullopt for
  // offer types other than card linked offers.
  base::Optional<CreditCard> card_;

  // The bubble and icon are sticky over a given set of origins. This is
  // populated when ShowOfferNotificationIfApplicable() is called and is cleared
  // when navigating to a origins outside of this set.
  std::vector<GURL> origins_to_display_bubble_;

  ObserverForTest* observer_for_testing_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_IMPL_H_
