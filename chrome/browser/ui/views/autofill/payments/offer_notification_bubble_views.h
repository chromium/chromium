// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}

namespace autofill {

// This class implements the Desktop bubble that displays any eligible credit
// card offers or rewards linked to the current page domain.
// The bubble has the following general layout.
//  ------------------------------------------------
// |  G Pay | Google Pay offer available         X |
// |                                               |
// |  Pay with Visa ****4545 at checkout           |
// |                                               |
// |                                   [Got it]    |
//  ------------------------------------------------
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
  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void Init() override;
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;
  void OnWidgetClosing(views::Widget* widget) override;

  PaymentsBubbleClosedReason closed_reason_ =
      PaymentsBubbleClosedReason::kUnknown;

  OfferNotificationBubbleController* controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_BUBBLE_VIEWS_H_
