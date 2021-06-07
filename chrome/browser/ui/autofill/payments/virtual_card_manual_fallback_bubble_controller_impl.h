// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_IMPL_H_

#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller.h"

#include "base/macros.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Implementation of per-tab class to control the virtual card manual fallback
// bubble and the omnibox icon.
class VirtualCardManualFallbackBubbleControllerImpl
    : public AutofillBubbleControllerBase,
      public VirtualCardManualFallbackBubbleController,
      public content::WebContentsUserData<
          VirtualCardManualFallbackBubbleControllerImpl> {
 public:
  class ObserverForTest {
   public:
    virtual void OnBubbleShown() = 0;
  };

  ~VirtualCardManualFallbackBubbleControllerImpl() override;
  VirtualCardManualFallbackBubbleControllerImpl(
      const VirtualCardManualFallbackBubbleControllerImpl&) = delete;
  VirtualCardManualFallbackBubbleControllerImpl& operator=(
      const VirtualCardManualFallbackBubbleControllerImpl&) = delete;

  // Show the bubble view.
  void ShowBubble(const CreditCard* virtual_card,
                  const std::u16string& virtual_card_cvc);

  // Invoked when the omnibox icon is clicked.
  void ReshowBubble();

  // VirtualCardManualFallbackBubbleController:
  AutofillBubbleBase* GetBubble() const override;
  std::u16string GetBubbleTitle() const override;
  std::u16string GetVirtualCardNumberFieldLabel() const override;
  std::u16string GetExpirationDateFieldLabel() const override;
  std::u16string GetCvcFieldLabel() const override;
  std::u16string GetCvc() const override;
  const CreditCard* GetVirtualCard() const override;
  bool ShouldIconBeVisible() const override;
  void OnBubbleClosed(PaymentsBubbleClosedReason closed_reason) override;

 protected:
  explicit VirtualCardManualFallbackBubbleControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<
      VirtualCardManualFallbackBubbleControllerImpl>;
  friend class VirtualCardManualFallbackBubbleViewsInteractiveUiTest;

  void SetEventObserverForTesting(ObserverForTest* observer_for_test);

  // The cvc of the virtual card.
  std::u16string virtual_card_cvc_;

  // The virtual card to be displayed to the user in the bubble.
  CreditCard virtual_card_;

  // Denotes whether the bubble is shown due to user gesture. If this is true,
  // it means the bubble is a reshown bubble.
  bool is_user_gesture_ = false;

  // Whether the omnibox icon for the bubble should be visible.
  bool should_icon_be_visible_ = false;

  ObserverForTest* observer_for_test_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CONTROLLER_IMPL_H_
