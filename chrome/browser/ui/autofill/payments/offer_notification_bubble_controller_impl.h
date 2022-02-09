// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/commerce/coupons/coupon_service_observer.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

struct AutofillOfferData;

// Implementation of per-tab class to control the offer notification bubble and
// Omnibox icon.
class OfferNotificationBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public OfferNotificationBubbleController,
      public content::WebContentsUserData<
          OfferNotificationBubbleControllerImpl>,
      public CouponServiceObserver {
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
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;
  void OnPromoCodeButtonClicked() override;

  // Displays an offer notification for the given |offer| on the current page.
  // The information of the |card|, if present, will be displayed in the bubble
  // for a card-linked offer.
  void ShowOfferNotificationIfApplicable(const AutofillOfferData* offer,
                                         const CreditCard* card);

  // Called when user clicks on omnibox icon.
  void ReshowBubble();

  // CouponService::CouponServiceObserver:
  void OnCouponInvalidated(
      const autofill::AutofillOfferData& offer_data) override;

 protected:
  explicit OfferNotificationBubbleControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase:
  void PrimaryPageChanged(content::Page& page) override;
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<
      OfferNotificationBubbleControllerImpl>;
  friend class OfferNotificationBubbleControllerImplTest;
  friend class OfferNotificationBubbleViewsTestBase;

  // Returns whether the web content associated with this controller is active.
  bool IsWebContentsActive();

  // For testing.
  void SetEventObserverForTesting(ObserverForTest* observer) {
    observer_for_testing_ = observer;
  }

  // Reset offer-related variables and hide all offer-related UIs.
  void ClearCurrentOffer();

  // The timestamp that the bubble has been shown. Used to check if the bubble
  // has been shown for longer than
  // kAutofillBubbleSurviveNavigationTime (5 seconds).
  base::Time bubble_shown_timestamp_;

  // The Autofill offer being displayed as a bubble. Set when the bubble is
  // requested to be shown via ShowOfferNotificationIfApplicable(~).
  raw_ptr<const AutofillOfferData> offer_;

  // Denotes whether the bubble is shown due to user gesture. If this is true,
  // it means the bubble is a reshown bubble.
  bool is_user_gesture_ = false;

  // The related credit card for a card linked offer. This can be nullopt for
  // offer types other than card linked offers.
  absl::optional<CreditCard> card_;

  // Denotes whether the promo code label button was clicked yet or not.
  // Determines the appropriate hover tooltip for the button.
  bool promo_code_button_clicked_ = false;

  // The bubble and icon are sticky over a given set of origins. This is
  // populated when ShowOfferNotificationIfApplicable() is called and is cleared
  // when navigating to a origins outside of this set, or when the corresponding
  // offer is no longer valid.
  std::vector<GURL> origins_to_display_bubble_;

  // Used to update coupon last display timestamp.
  raw_ptr<CouponService> coupon_service_;

  raw_ptr<ObserverForTest> observer_for_testing_ = nullptr;

  base::ScopedObservation<CouponService, CouponServiceObserver>
      coupon_service_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_CONTROLLER_IMPL_H_
