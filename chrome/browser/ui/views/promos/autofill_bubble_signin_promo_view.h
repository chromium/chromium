// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_AUTOFILL_BUBBLE_SIGNIN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_AUTOFILL_BUBBLE_SIGNIN_PROMO_VIEW_H_

#include <memory>

#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/autofill/autofill_bubble_signin_promo_controller.h"
#include "chrome/browser/ui/signin/bubble_signin_promo_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

namespace signin_metrics {
enum class AccessPoint;
}
namespace content {
class WebContents;
}
namespace autofill {
class AutofillBubbleSignInPromoController;
}
class AutofillBubbleSignInPromoView;

// A view that can show up after saving a piece of autofill data without being
// signed in to offer signing users in so they can access their credentials
// across devices.
class AutofillBubbleSignInPromoView : public views::View {
  METADATA_HEADER(AutofillBubbleSignInPromoView, views::View)

 public:
  explicit AutofillBubbleSignInPromoView(
      content::WebContents* web_contents,
      signin_metrics::AccessPoint access_point,
      base::OnceCallback<void(content::WebContents*)> move_callback);
  AutofillBubbleSignInPromoView(const AutofillBubbleSignInPromoView&) = delete;
  AutofillBubbleSignInPromoView& operator=(
      const AutofillBubbleSignInPromoView&) = delete;
  ~AutofillBubbleSignInPromoView() override;

  // Records that the bubble has been dismissed.
  static void RecordSignInPromoDismissed(content::WebContents* web_contents);

 private:
  // Delegate for the personalized sign in promo view used when desktop identity
  // consistency is enabled.
  class DiceSigninPromoDelegate;

  autofill::AutofillBubbleSignInPromoController controller_;
  const signin_metrics::AccessPoint access_point_;
  std::unique_ptr<DiceSigninPromoDelegate> dice_sign_in_promo_delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_AUTOFILL_BUBBLE_SIGNIN_PROMO_VIEW_H_
