// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/promos/bubble_signin_promo_signin_button_view.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace {

constexpr base::TimeDelta kDoubleClickSignInPreventionDelay =
    base::Seconds(0.5);

}  // namespace

BubbleSignInPromoSignInButtonView::BubbleSignInPromoSignInButtonView(
    views::Button::PressedCallback callback,
    ui::ButtonStyle button_style)
    : account_(std::nullopt) {
  views::Builder<BubbleSignInPromoSignInButtonView>(this)
      .SetUseDefaultFillLayout(true)
      .AddChild(
          // Regular MD text button when there is no account.
          views::Builder<views::MdTextButton>()
              .SetCallback(std::move(callback))
              .SetText(
                  l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON))
              .SetStyle(button_style))
      .BuildChildren();

  SetProperty(views::kElementIdentifierKey, kPromoSignInButton);
}

BubbleSignInPromoSignInButtonView::BubbleSignInPromoSignInButtonView(
    const AccountInfo& account,
    const gfx::Image& account_icon,
    views::Button::PressedCallback callback,
    signin_metrics::AccessPoint access_point,
    bool use_account_name_as_title)
    : account_(account) {
  DCHECK(!account_icon.IsEmpty());
  auto card_title =
      use_account_name_as_title
          ? base::UTF8ToUTF16(account.full_name)
          : l10n_util::GetStringUTF16(IDS_PROFILES_DICE_NOT_SYNCING_TITLE);

  const views::BoxLayout::Orientation orientation =
      views::BoxLayout::Orientation::kHorizontal;

  std::unique_ptr<views::BoxLayout> button_layout =
      std::make_unique<views::BoxLayout>(orientation, gfx::Insets(), 16);

  std::unique_ptr<HoverButton> hover_button = std::make_unique<HoverButton>(
      views::Button::PressedCallback(),
      std::make_unique<BadgedProfilePhoto>(
          BadgedProfilePhoto::BADGE_TYPE_SYNC_OFF, account_icon),
      card_title, base::ASCIIToUTF16(account_->email));
  raw_ptr<views::MdTextButton> text_button = nullptr;

  if (orientation == views::BoxLayout::Orientation::kHorizontal) {
    button_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    hover_button->SetProperty(views::kBoxLayoutFlexKey,
                              views::BoxLayoutFlexSpecification());
  }

  views::Builder<BubbleSignInPromoSignInButtonView>(this)
      .SetLayoutManager(std::move(button_layout))
      .AddChildren(views::Builder<HoverButton>(std::move(hover_button))
                       .SetBorder(std::unique_ptr<views::Border>(nullptr))
                       .SetEnabled(false),
                   views::Builder<views::MdTextButton>()
                       .SetText(l10n_util::GetStringUTF16(
                           IDS_PROFILES_DICE_SIGNIN_BUTTON))
                       .SetStyle(ui::ButtonStyle::kProminent)
                       .CopyAddressTo(&text_button))
      .BuildChildren();

  // If the promo is triggered from an autofill bubble, ignore any interaction
  // with the sign in button at first, because the button for an autofill data
  // save is in the same place as the button for performing a direct sign in
  // with an existing account. If a user double clicked on the save button, it
  // would therefore sign them in directly. The delayed adding of the callback
  // to the button avoids that.
  bool disable_first =
      access_point == signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE;

  if (disable_first) {
    // Add the callback to the button after the delay.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &BubbleSignInPromoSignInButtonView::AddCallbackToSignInButton,
            weak_ptr_factory_.GetWeakPtr(), text_button, std::move(callback)),
        kDoubleClickSignInPreventionDelay);
  } else {
    AddCallbackToSignInButton(text_button, std::move(callback));
  }

  SetProperty(views::kElementIdentifierKey, kPromoSignInButton);
}

void BubbleSignInPromoSignInButtonView::AddCallbackToSignInButton(
    views::MdTextButton* text_button,
    views::Button::PressedCallback callback) {
  text_button->SetCallback(std::move(callback));

  // Triggers an event for testing.
  views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      kBubbleSignInPromoSignInButtonHasCallback, this);
}

BubbleSignInPromoSignInButtonView::
    ~BubbleSignInPromoSignInButtonView() = default;

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleSignInPromoSignInButtonHasCallback);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BubbleSignInPromoSignInButtonView,
                                      kPromoSignInButton);

BEGIN_METADATA(BubbleSignInPromoSignInButtonView)
END_METADATA
