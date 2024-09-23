// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_BUBBLE_SIGNIN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_BUBBLE_SIGNIN_PROMO_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/signin/bubble_signin_promo_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

class Profile;
class BubbleSignInPromoSignInButtonView;

// A personalized sign in promo used when Desktop Identity Consistency is
// enabled. Its display a message informing the user the benefits of signing in
// and a button that allows the user to sign in. The button has 2 different
// displays:
// * If Chrome has no accounts, then the promo button is a MD button allowing
//   the user to sign in to Chrome.
// * If Chrome has at least one account in web, then the promo button is
// personalized with the user full name and avatar icon and allows the user to
// sign in to Chrome.
// * If Chrome is in sign in pending state, then the promo is personalized with
// the user full name and avatar icon and allows the user to reauth.
class BubbleSignInPromoView : public views::View {
  METADATA_HEADER(BubbleSignInPromoView, views::View)

 public:
  // Creates a personalized sign in promo view.
  // |delegate| is not owned by BubbleSignInPromoView.
  // The promo message is set to |accounts_promo_message_resource_id| when
  // Chrome has at least one account.
  // |button_style| is used to style non-personalized signin button. Otherwise,
  // the button remains prominent.
  // The promo message is set in a font given by |text_style|. It is defaulted
  // to a primary style font.
  BubbleSignInPromoView(
      Profile* profile,
      BubbleSignInPromoDelegate* delegate,
      signin_metrics::AccessPoint access_point,
      int accounts_promo_message_resource_id = 0,
      ui::ButtonStyle button_style = ui::ButtonStyle::kProminent,
      int text_style = views::style::STYLE_PRIMARY);
  BubbleSignInPromoView(const BubbleSignInPromoView&) = delete;
  BubbleSignInPromoView& operator=(const BubbleSignInPromoView&) = delete;
  ~BubbleSignInPromoView() override;

 private:
  // Used to sign in in the DiceAccountsMenu and when |signin_button_| is
  // pressed.
  void SignIn();

  // Delegate, to handle clicks on the sign-in buttons.
  raw_ptr<BubbleSignInPromoDelegate, DanglingUntriaged> delegate_;
  raw_ptr<BubbleSignInPromoSignInButtonView> signin_button_view_ = nullptr;
};
#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_BUBBLE_SIGNIN_PROMO_VIEW_H_
