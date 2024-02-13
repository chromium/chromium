// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_BUBBLE_SIGNIN_PROMO_SIGNIN_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_BUBBLE_SIGNIN_PROMO_SIGNIN_BUTTON_VIEW_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

// Sign-in button view used by Sign in promos that presents the
// account information (avatar image and email) and allows the user to
// sign in to Chrome.
class BubbleSignInPromoSignInButtonView : public views::View {
  METADATA_HEADER(BubbleSignInPromoSignInButtonView, views::View)

 public:
  // Create a non-personalized sign-in button with |button_style|.
  // |callback| is called every time the user interacts with this button.
  explicit BubbleSignInPromoSignInButtonView(
      views::Button::PressedCallback callback,
      ui::ButtonStyle button_style);

  // Creates a sign-in button personalized with the data from |account|.
  // |callback| is called every time the user interacts with this button.
  BubbleSignInPromoSignInButtonView(const AccountInfo& account_info,
                                  const gfx::Image& account_icon,
                                  views::Button::PressedCallback callback,
                                  bool use_account_name_as_title = false);
  BubbleSignInPromoSignInButtonView(const BubbleSignInPromoSignInButtonView&) =
      delete;
  BubbleSignInPromoSignInButtonView& operator=(
      const BubbleSignInPromoSignInButtonView&) = delete;
  ~BubbleSignInPromoSignInButtonView() override;

  std::optional<AccountInfo> account() const { return account_; }

 private:
  const std::optional<AccountInfo> account_;
};

BEGIN_VIEW_BUILDER(, BubbleSignInPromoSignInButtonView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, BubbleSignInPromoSignInButtonView)

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_BUBBLE_SIGNIN_PROMO_SIGNIN_BUTTON_VIEW_H_
