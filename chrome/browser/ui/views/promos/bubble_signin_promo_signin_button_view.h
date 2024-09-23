// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_BUBBLE_SIGNIN_PROMO_SIGNIN_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_BUBBLE_SIGNIN_PROMO_SIGNIN_BUTTON_VIEW_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleSignInPromoSignInButtonHasCallback);

// Sign-in button view used by Sign in promos that presents the
// account information (avatar image and email) and allows the user to
// sign in to Chrome.
class BubbleSignInPromoSignInButtonView : public views::View {
  METADATA_HEADER(BubbleSignInPromoSignInButtonView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPromoSignInButton);

  // Create a non-personalized sign-in button with |button_style|.
  // |callback| is called every time the user interacts with this button.
  explicit BubbleSignInPromoSignInButtonView(
      views::Button::PressedCallback callback,
      bool is_autofill_promo,
      ui::ButtonStyle button_style,
      std::u16string button_text);

  // Creates a sign-in button personalized with the data from |account|.
  // |callback| is called every time the user interacts with this button.
  BubbleSignInPromoSignInButtonView(const AccountInfo& account_info,
                                    const gfx::Image& account_icon,
                                    views::Button::PressedCallback callback,
                                    bool is_autofill_promo,
                                    std::u16string button_text,
                                    std::u16string button_accessibility_text);
  BubbleSignInPromoSignInButtonView(const BubbleSignInPromoSignInButtonView&) =
      delete;
  BubbleSignInPromoSignInButtonView& operator=(
      const BubbleSignInPromoSignInButtonView&) = delete;
  ~BubbleSignInPromoSignInButtonView() override;

  std::optional<AccountInfo> account() const { return account_; }

 private:
  // Calls `AddCallbackToSignInButton`, but adds a delay if it is for an
  // autofill sign in promo in order to avoid a direct action through
  // double click on the save button in the bubble before.
  void AddOrDelayCallbackForSignInButton(
      views::MdTextButton* text_button,
      views::Button::PressedCallback& callback,
      bool is_autofill_promo);

  void AddCallbackToSignInButton(views::MdTextButton* text_button,
                                 views::Button::PressedCallback callback);

  const std::optional<AccountInfo> account_;

  base::WeakPtrFactory<BubbleSignInPromoSignInButtonView> weak_ptr_factory_{
      this};
};

BEGIN_VIEW_BUILDER(, BubbleSignInPromoSignInButtonView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(, BubbleSignInPromoSignInButtonView)

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_BUBBLE_SIGNIN_PROMO_SIGNIN_BUTTON_VIEW_H_
