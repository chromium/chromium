// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"

namespace autofill {

class PaymentsChurnedUsersBubbleController;

class PaymentsChurnedUsersBubbleView : public AutofillLocationBarBubble {
  METADATA_HEADER(PaymentsChurnedUsersBubbleView, AutofillLocationBarBubble)

 public:
  PaymentsChurnedUsersBubbleView(
      views::BubbleAnchor anchor,
      content::WebContents* web_contents,
      PaymentsChurnedUsersBubbleController* controller);
  PaymentsChurnedUsersBubbleView(const PaymentsChurnedUsersBubbleView&) =
      delete;
  PaymentsChurnedUsersBubbleView& operator=(
      const PaymentsChurnedUsersBubbleView&) = delete;
  ~PaymentsChurnedUsersBubbleView() override;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

 private:
  // LocationBarBubbleDelegateView:
  void Init() override;

  raw_ptr<PaymentsChurnedUsersBubbleController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_BUBBLE_VIEW_H_
