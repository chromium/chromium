// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/styled_label.h"

namespace content {
class WebContents;
}

namespace autofill {

class PromoCodeLabelButton;
class PromoCodeLabelView;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kOfferNotificationBubbleElementId);

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
  // TODO(crbug.com/1507113) : Remove these friended test and convert the test
  // to use Kombucha framework.
  FRIEND_TEST_ALL_PREFIXES(OfferNotificationBubbleViewsInteractiveUiTest,
                           CopyPromoCode);
  FRIEND_TEST_ALL_PREFIXES(
      OfferNotificationBubbleViewsInteractiveUiTest,
      ReshowOfferNotificationBubble_OfferDeletedBetweenShows);
  FRIEND_TEST_ALL_PREFIXES(OfferNotificationBubbleViewsInteractiveUiTest,
                           ShowGPayPromoCodeBubble);
  FRIEND_TEST_ALL_PREFIXES(
      OfferNotificationBubbleViewsInteractiveUiTest,
      ShowGPayPromoCodeOffer_WhenGPayPromoCodeOfferAndShoppingServiceOfferAreBothAvailable);
  FRIEND_TEST_ALL_PREFIXES(
      OfferNotificationBubbleViewsInteractiveUiTest,
      ShowShoppingServiceFreeListingOffer_RecordHistoryClusterUsageRelatedMetrics);
  FRIEND_TEST_ALL_PREFIXES(
      OfferNotificationBubbleViewsInteractiveUiTest,
      ShowShoppingServiceFreeListingOffer_WhenGPayPromoCodeOfferNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(OfferNotificationBubbleViewsInteractiveUiTest,
                           TooltipAndAccessibleName);
  FRIEND_TEST_ALL_PREFIXES(OfferNotificationBubbleViewsInteractiveUiTest,
                           ShowTermsAndConditionsPage);
  FRIEND_TEST_ALL_PREFIXES(
      OfferNotificationBubbleViewsWithDiscountOnChromeHistoryClusterTest,
      RecordHistoryClusterUsageRelatedMetrics);
  FRIEND_TEST_ALL_PREFIXES(
      OfferNotificationBubbleViewsWithDiscountOnChromeHistoryClusterTest,
      ShowShoppingServiceFreeListingOffer_WhenNavigatedFromChromeHistoryCluster);

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

  void OpenTermsAndConditionsPage(AutofillOfferData offer,
                                  std::string seller_domain);

  std::unique_ptr<views::View> CreateFreeListingCouponOfferMainPageHeaderView();
  std::unique_ptr<views::View> CreateFreeListingCouponOfferMainPageTitleView(
      const AutofillOfferData& offer);
  std::unique_ptr<views::View> CreateFreeListingCouponOfferMainPageContent(
      const AutofillOfferData& offer,
      const std::string& seller_domain);
  void OpenFreeListingCouponOfferMainPage(AutofillOfferData offer,
                                          std::string seller_domain);

  void ResetPointersToFreeListingCouponOfferMainPageContent();

  raw_ptr<OfferNotificationBubbleController> controller_;

  // TODO(crbug.com/1334806): Replace tests with Pixel tests.
  raw_ptr<views::StyledLabel> promo_code_label_ = nullptr;

  raw_ptr<views::Label> instructions_label_ = nullptr;

  // Used in tests for FreeListing offers.
  raw_ptr<PromoCodeLabelButton> promo_code_label_button_ = nullptr;
  raw_ptr<PromoCodeLabelView> promo_code_label_view_ = nullptr;
  raw_ptr<views::StyledLabel> promo_code_value_prop_label_ = nullptr;

  raw_ptr<PageSwitcherView> free_listing_coupon_page_container_ = nullptr;

  base::WeakPtrFactory<OfferNotificationBubbleViews> weak_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_
