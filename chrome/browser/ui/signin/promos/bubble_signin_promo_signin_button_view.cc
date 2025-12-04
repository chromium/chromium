// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/promos/bubble_signin_promo_signin_button_view.h"

#include <memory>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/profiles/badged_profile_photo.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
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
    signin_metrics::AccessPoint access_point,
    ui::ButtonStyle button_style,
    std::u16string button_text)
    : account_(std::nullopt) {
  // Regular MD text button when there is no account.
  views::Builder<views::MdTextButton> button_builder;
  button_builder.SetText(button_text)
      .SetStyle(button_style)
      .CopyAddressTo(&text_button_);
  // If the `button_style` has a white background by default
  // (`ui::ButtonStyle::kDefault` and `ui::ButtonStyle::kText`), override it
  // with the neutral color.
  if (button_style == ui::ButtonStyle::kDefault ||
      button_style == ui::ButtonStyle::kText) {
    button_builder.SetBgColorIdOverride(ui::kColorSysNeutralContainer);
  }

  views::Builder<BubbleSignInPromoSignInButtonView>(this)
      .SetUseDefaultFillLayout(true)
      .AddChild(std::move(button_builder))
      .BuildChildren();

  // Add the callback to the button with a delay if it is a bubble sign in
  // promo.
  AddOrDelayCallbackForSignInButton(callback,
                                    signin::IsBubbleSigninPromo(access_point));

  SetProperty(views::kElementIdentifierKey, kPromoSignInButton);
}

BubbleSignInPromoSignInButtonView::BubbleSignInPromoSignInButtonView(
    const AccountInfo& account,
    const gfx::Image& account_icon,
    views::Button::PressedCallback callback,
    signin_metrics::AccessPoint access_point,
    std::u16string button_text,
    std::u16string button_accessibility_text)
    : account_(account) {
  DCHECK(!account_icon.IsEmpty());
  auto card_title = base::UTF8ToUTF16(account.full_name);

  bool is_signin_promo = signin::IsSignInPromo(access_point);
  bool is_bubble_promo = signin::IsBubbleSigninPromo(access_point);

  const views::BoxLayout::Orientation orientation =
      is_signin_promo ? views::BoxLayout::Orientation::kVertical
                      : views::BoxLayout::Orientation::kHorizontal;

  std::unique_ptr<views::BoxLayout> button_layout =
      std::make_unique<views::BoxLayout>(
          orientation, gfx::Insets(),
          views::LayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_VERTICAL));

  // Don't show a sync badge if this promo is only for a signin.
  std::unique_ptr<HoverButton> hover_button = std::make_unique<HoverButton>(
      views::Button::PressedCallback(),
      std::make_unique<BadgedProfilePhoto>(
          is_signin_promo ? BadgedProfilePhoto::BadgeType::kNone
                          : BadgedProfilePhoto::BadgeType::kSyncOff,
          account_icon),
      card_title, base::ASCIIToUTF16(account_->email));

  hover_button->SetProperty(views::kBoxLayoutFlexKey,
                            views::BoxLayoutFlexSpecification());
  hover_button->SetBorder(nullptr);

  views::BoxLayout::CrossAxisAlignment alignment =
      views::BoxLayout::CrossAxisAlignment::kCenter;
  if (orientation == views::BoxLayout::Orientation::kVertical) {
    hover_button->SetSubtitleTextStyle(views::style::CONTEXT_LABEL,
                                       views::style::STYLE_SECONDARY);

    // This will place the sign in button at the horizontal end of the bubble.
    alignment = views::BoxLayout::CrossAxisAlignment::kEnd;
  }
  button_layout->set_cross_axis_alignment(alignment);

  views::Builder<BubbleSignInPromoSignInButtonView>(this)
      .SetLayoutManager(std::move(button_layout))
      .AddChildren(views::Builder<HoverButton>(std::move(hover_button))
                       .SetEnabled(false),
                   views::Builder<views::MdTextButton>()
                       .SetText(button_text)
                       .SetStyle(ui::ButtonStyle::kProminent)
                       .CopyAddressTo(&text_button_))
      .BuildChildren();

  if (!button_accessibility_text.empty()) {
    text_button_->GetViewAccessibility().SetName(
        std::move(button_accessibility_text));
  }

  // Add the callback to the button with a delay if it is a bubble sign in
  // promo.
  AddOrDelayCallbackForSignInButton(callback, is_bubble_promo);

  SetProperty(views::kElementIdentifierKey, kPromoSignInButton);
}

views::View* BubbleSignInPromoSignInButtonView::GetSignInButton() const {
  return text_button_ ? text_button_ : nullptr;
}

void BubbleSignInPromoSignInButtonView::AddOrDelayCallbackForSignInButton(
    views::Button::PressedCallback& callback,
    bool is_bubble_promo) {
  // If the promo is shown in a separate sign in promo bubble, ignore any
  // interaction with the sign in button at first, because the button for a data
  // save might be in the same place as the sign in button. If a user double
  // clicked on the save button, it would therefore sign them in directly, or
  // redirect them to a sign in page. The delayed adding of the callback to the
  // button avoids that.
  if (is_bubble_promo) {
    // Add the callback to the button after the delay.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &BubbleSignInPromoSignInButtonView::AddCallbackToSignInButton,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        kDoubleClickSignInPreventionDelay);
  } else {
    // Add the callback to the button immediately.
    AddCallbackToSignInButton(std::move(callback));
  }
}

void BubbleSignInPromoSignInButtonView::AddCallbackToSignInButton(
    views::Button::PressedCallback callback) {
  text_button_->SetCallback(std::move(callback));

  // Triggers an event for testing.
  if (GetWidget()) {
    views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
        kBubbleSignInPromoSignInButtonHasCallback, this);
  }
}

BubbleSignInPromoSignInButtonView::~BubbleSignInPromoSignInButtonView() =
    default;

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleSignInPromoSignInButtonHasCallback);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BubbleSignInPromoSignInButtonView,
                                      kPromoSignInButton);

BEGIN_METADATA(BubbleSignInPromoSignInButtonView)
END_METADATA
