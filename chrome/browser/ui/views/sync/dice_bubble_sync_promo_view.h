// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_DICE_BUBBLE_SYNC_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_DICE_BUBBLE_SYNC_PROMO_VIEW_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/views/profiles/dice_accounts_menu.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "ui/views/controls/button/button.h"
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
class DiceBubbleSyncPromoView : public views::View,
                                public views::ButtonListener {
 public:
  // Creates a personalized sync promo view.
  // |delegate| is not owned by DiceBubbleSyncPromoView.
  // The promo message is set to |no_accounts_promo_message_resource_id| when
  // Chrome has no accounts. If no value is given, then no message is shown.
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
                          int no_accounts_promo_message_resource_id = 0,
                          int accounts_promo_message_resource_id = 0,
                          bool signin_button_prominent = true,
                          int text_style = views::style::STYLE_PRIMARY);
  ~DiceBubbleSyncPromoView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Returns the sign-in button.
  views::View* GetSigninButtonForTesting();

 private:
  // Used to enable sync in the DiceAccountsMenu and when |signin_button_| is
  // pressed.
  void EnableSync(bool is_default_promo_account,
                  const base::Optional<AccountInfo>& account);

  // views::View:
  const char* GetClassName() const override;

  // Delegate, to handle clicks on the sign-in buttons.
  BubbleSyncPromoDelegate* delegate_;
  DiceSigninButtonView* signin_button_view_ = nullptr;

  // Accounts submenu that is shown when |signin_button_->drop_down_arrow()| is
  // pressed.
  std::unique_ptr<DiceAccountsMenu> dice_accounts_menu_;

  std::vector<AccountInfo> accounts_for_submenu_;
  std::vector<gfx::Image> images_for_submenu_;

  DISALLOW_COPY_AND_ASSIGN(DiceBubbleSyncPromoView);
};
#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_DICE_BUBBLE_SYNC_PROMO_VIEW_H_
