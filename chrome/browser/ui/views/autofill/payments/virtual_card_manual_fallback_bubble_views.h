// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_VIEWS_H_

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}

namespace views {
class MdTextButton;
}

namespace autofill {

// This class implements the desktop bubble that displays the information of the
// virtual card that was sent to Chrome from Payments.
class VirtualCardManualFallbackBubbleViews
    : public AutofillBubbleBase,
      public LocationBarBubbleDelegateView {
 public:
  // The bubble will be anchored to the |anchor_view|.
  VirtualCardManualFallbackBubbleViews(
      views::View* anchor_view,
      content::WebContents* web_contents,
      VirtualCardManualFallbackBubbleController* controller);
  ~VirtualCardManualFallbackBubbleViews() override;
  VirtualCardManualFallbackBubbleViews(
      const VirtualCardManualFallbackBubbleViews&) = delete;
  VirtualCardManualFallbackBubbleViews& operator=(
      const VirtualCardManualFallbackBubbleViews&) = delete;

 private:
  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void Init() override;
  ui::ImageModel GetWindowIcon() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;
  void OnWidgetClosing(views::Widget* widget) override;

  // Creates a button for the |field|. If the button is pressed, the text of it
  // will be copied to the clipboard.
  std::unique_ptr<views::MdTextButton> CreateRowItemButtonForField(
      VirtualCardManualFallbackBubbleField field);

  VirtualCardManualFallbackBubbleController* controller_;

  PaymentsBubbleClosedReason closed_reason_ =
      PaymentsBubbleClosedReason::kUnknown;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_VIEWS_H_
