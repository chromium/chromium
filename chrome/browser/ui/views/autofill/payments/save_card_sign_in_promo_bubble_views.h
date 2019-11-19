// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_SIGN_IN_PROMO_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_SIGN_IN_PROMO_BUBBLE_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"

namespace content {
class WebContents;
}

namespace autofill {

// This class displays the Sign-in/Sync promo bubble which is shown after the
// user saves a credit card locally to Autofill. It contains a title with a
// promo message to the user (this title is handled by the controller) and
// either a large sign-in button or a sync-promo button.
class SaveCardSignInPromoBubbleViews : public SaveCardBubbleViews {
 public:
  // Bubble will be anchored to |anchor_view|.
  SaveCardSignInPromoBubbleViews(views::View* anchor_view,
                                 content::WebContents* web_contents,
                                 SaveCardBubbleController* controller);

 private:
  std::unique_ptr<views::View> CreateMainContentView() override;

  ~SaveCardSignInPromoBubbleViews() override;

  DISALLOW_COPY_AND_ASSIGN(SaveCardSignInPromoBubbleViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_CARD_SIGN_IN_PROMO_BUBBLE_VIEWS_H_
