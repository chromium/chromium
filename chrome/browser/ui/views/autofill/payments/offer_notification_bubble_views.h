// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/styled_label.h"

namespace content {
class WebContents;
}

namespace autofill {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kOfferNotificationBubbleElementId);

// This class implements the Desktop bubble that displays any eligible offers or
// rewards linked to the current page domain. This can include card-linked
// offers, for which "Pay with [card] at checkout" is shown, merchant promo
// code offers, which shows the code the user should apply at checkout, or
// offers saved to the user's Google Wallet, which shows the offer's short title
// and a link to its terms and conditions.
// TODO(crbug.com/546252995): Deprecate the card-linked and GPay promo code
// bubbles since Wallet direct offers are the only supported offer type.
class OfferNotificationBubbleViews : public AutofillLocationBarBubble {
  METADATA_HEADER(OfferNotificationBubbleViews, AutofillLocationBarBubble)
 public:
  // Bubble will be anchored to |anchor_view|.
  OfferNotificationBubbleViews(views::BubbleAnchor anchor_view,
                               content::WebContents* web_contents,
                               OfferNotificationBubbleController* controller);
  OfferNotificationBubbleViews(const OfferNotificationBubbleViews&) = delete;
  OfferNotificationBubbleViews& operator=(const OfferNotificationBubbleViews&) =
      delete;
  ~OfferNotificationBubbleViews() override;

 private:
  // TODO(crbug.com/40947801) : Remove these friended test and convert the test
  // to use Kombucha framework.
  FRIEND_TEST_ALL_PREFIXES(OfferNotificationBubbleViewsInteractiveUiTest,
                           CopyPromoCode);
  FRIEND_TEST_ALL_PREFIXES(
      OfferNotificationBubbleViewsInteractiveUiTest,
      ReshowOfferNotificationBubble_OfferDeletedBetweenShows);
  FRIEND_TEST_ALL_PREFIXES(OfferNotificationBubbleViewsInteractiveUiTest,
                           ShowGPayPromoCodeBubble);
  FRIEND_TEST_ALL_PREFIXES(OfferNotificationBubbleViewsInteractiveUiTest,
                           ShowWalletDirectOfferBubble);
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
  void InitWithGPayPromoCodeOfferContent();
  void InitWithWalletDirectOfferContent();

  // Called when the link of the offer's body text is clicked. Browser will
  // switch to a new tab with the offer details url.
  void OnOfferDetailsLinkClicked();

  raw_ptr<OfferNotificationBubbleController> controller_;

  // TODO(crbug.com/40228302): Replace tests with Pixel tests.
  raw_ptr<views::StyledLabel> promo_code_label_ = nullptr;

  raw_ptr<views::Label> instructions_label_ = nullptr;

  // Body text of the Wallet direct offer bubble, which ends with a link to the
  // terms and conditions of the offer.
  raw_ptr<views::StyledLabel> wallet_direct_offer_label_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_
