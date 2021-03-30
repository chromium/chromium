// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_DICE_BUBBLE_SYNC_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_DICE_BUBBLE_SYNC_PROMO_VIEW_H_

#include <memory>
#include <vector>

#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

class Profile;
class DiceSigninButtonView;

// A personalized sync promo used when Desktop Identity Consistency is enabled.
// Its display a message informing the user the benefits of enabling sync and
// a button that allows the user to enable sync.
// The button has 2 different displays:
// * If Chrome has no accounts, then the promo button is a MD button allowing
//   the user to sign in to Chrome.
// * If Chrome has at least one account, then the promo button is personalized
//   with the user full name and avatar icon and allows the user to enable sync.
class DiceBubbleSyncPromoView : public views::View {
 public:
  METADATA_HEADER(DiceBubbleSyncPromoView);
  // Creates a personalized sync promo view.
  // |delegate| is not owned by DiceBubbleSyncPromoView.
  // The promo message is set to |accounts_promo_message_resource_id| when
  // Chrome has at least one account.
  // If |signin_button_prominent| is false and a non-personalized signin button
  // is shown, the button is set to non-prominent. Otherwise the button remains
  // prominent.
  // The promo message is set in a font given by |text_style|. It is defaulted
  // to a primary style font.
  DiceBubbleSyncPromoView(Profile* profile,
                          BubbleSyncPromoDelegate* delegate,
                          signin_metrics::AccessPoint access_point,
                          int accounts_promo_message_resource_id = 0,
                          bool signin_button_prominent = true,
                          int text_style = views::style::STYLE_PRIMARY);
  DiceBubbleSyncPromoView(const DiceBubbleSyncPromoView&) = delete;
  DiceBubbleSyncPromoView& operator=(const DiceBubbleSyncPromoView&) = delete;
  ~DiceBubbleSyncPromoView() override;

  // Returns the sign-in button.
  views::View* GetSigninButtonForTesting();

 private:
  // Used to enable sync in the DiceAccountsMenu and when |signin_button_| is
  // pressed.
  void EnableSync();

  // Delegate, to handle clicks on the sign-in buttons.
  BubbleSyncPromoDelegate* delegate_;
  DiceSigninButtonView* signin_button_view_ = nullptr;
};
#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_DICE_BUBBLE_SYNC_PROMO_VIEW_H_
