// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_OPT_IN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_OPT_IN_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"

namespace autofill {

class MandatoryReauthOptInBubbleView : public AutofillLocationBarBubble {
 public:
  MandatoryReauthOptInBubbleView(views::View* anchor_view,
                                 content::WebContents* web_contents,
                                 MandatoryReauthBubbleController* controller);
  MandatoryReauthOptInBubbleView(const MandatoryReauthOptInBubbleView&) =
      delete;
  MandatoryReauthOptInBubbleView& operator=(
      const MandatoryReauthOptInBubbleView&) = delete;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

 protected:
  ~MandatoryReauthOptInBubbleView() override;

  // LocationBarBubbleDelegateView:
  void Init() override;

 private:
  raw_ptr<MandatoryReauthBubbleController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_MANDATORY_REAUTH_OPT_IN_BUBBLE_VIEW_H_
