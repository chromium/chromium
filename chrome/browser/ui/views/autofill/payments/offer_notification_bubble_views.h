// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/views/controls/styled_label.h"

namespace content {
class WebContents;
}

namespace autofill {

class PromoCodeLabelButton;

// This class implements the Desktop bubble that displays any eligible offers or
// rewards linked to the current page domain. This can include card-linked
// offers, for which "Pay with [card] at checkout" is shown, or merchant promo
// code offers, which shows the code the user should apply at checkout.
class OfferNotificationBubbleViews : public AutofillBubbleBase,
                                     public LocationBarBubbleDelegateView {
 public:
  // Bubble will be anchored to |anchor_view|.
  OfferNotificationBubbleViews(views::View* anchor_view,
                               content::WebContents* web_contents,
                               OfferNotificationBubbleController* controller);
  ~OfferNotificationBubbleViews() override;
  OfferNotificationBubbleViews(const OfferNotificationBubbleViews&) = delete;
  OfferNotificationBubbleViews& operator=(const OfferNotificationBubbleViews&) =
      delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(OfferNotificationBubbleViewsInteractiveUiTest,
                           CopyPromoCode);
  FRIEND_TEST_ALL_PREFIXES(
      OfferNotificationBubbleViewsInteractiveUiTest,
      ReshowOfferNotificationBubble_OfferDeletedBetweenShows);
  FRIEND_TEST_ALL_PREFIXES(OfferNotificationBubbleViewsInteractiveUiTest,
                           ShowGPayPromoCodeBubble);
  FRIEND_TEST_ALL_PREFIXES(OfferNotificationBubbleViewsInteractiveUiTest,
                           TooltipAndAccessibleName);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void Init() override;
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;
  void OnWidgetDestroying(views::Widget* widget) override;

  void InitWithCardLinkedOfferContent();
  void InitWithFreeListingCouponOfferContent();
  void InitWithGPayPromoCodeOfferContent();

  // Called when the promo code LabelButton is clicked for a promo code offer.
  // Copies the promo code to the clipboard and updates the button tooltip.
  void OnPromoCodeButtonClicked();

  // Called when the See Details link of the value prop text is clicked.
  // Browser will switch to a new tab with the offer details url.
  void OnPromoCodeSeeDetailsClicked();

  void UpdateButtonTooltipsAndAccessibleNames();

  raw_ptr<OfferNotificationBubbleController> controller_;

  raw_ptr<PromoCodeLabelButton> promo_code_label_button_ = nullptr;

  // TODO(crbug.com/1334806): Replace tests with Pixel tests.
  raw_ptr<views::StyledLabel> promo_code_label_ = nullptr;

  raw_ptr<views::Label> instructions_label_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_
