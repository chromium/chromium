// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/promos/bubble_signin_promo_view.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_sync_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/promos/bubble_signin_promo_signin_button_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"

using signin_util::SignedInState;

namespace {

constexpr int kTitleMaxWidth = 218;

int GetSubtitleID(signin_metrics::AccessPoint access_point,
                  SignedInState signed_in_state,
                  int default_subtitle_id) {
  if (access_point ==
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE) {
    switch (signed_in_state) {
      case SignedInState::kSignedOut:
      case SignedInState::kWebOnlySignedIn:
        return IDS_AUTOFILL_SIGNIN_PROMO_SUBTITLE_PASSWORD;
      case SignedInState::kSignInPending:
        return IDS_AUTOFILL_VERIFY_PROMO_SUBTITLE_PASSWORD;
      default:
        break;
    }
  }

  if (access_point ==
      signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE) {
    switch (signed_in_state) {
      case SignedInState::kSignedOut:
      case SignedInState::kWebOnlySignedIn:
        return IDS_AUTOFILL_SIGNIN_PROMO_SUBTITLE_ADDRESS;
      case SignedInState::kSignInPending:
        return IDS_AUTOFILL_VERIFY_PROMO_SUBTITLE_ADDRESS;
      default:
        break;
    }
  }

  return default_subtitle_id;
}

std::u16string GetButtonText(bool is_autofill_promo,
                             SignedInState signed_in_state,
                             const std::string& name) {
  if (is_autofill_promo) {
    switch (signed_in_state) {
      case SignedInState::kSignedOut:
        return l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_BUTTON);
      case SignedInState::kWebOnlySignedIn:
        return l10n_util::GetStringFUTF16(
            IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_ACCEPT_TEXT,
            {base::UTF8ToUTF16(name)});
      case SignedInState::kSignInPending:
        return l10n_util::GetStringUTF16(IDS_PROFILES_VERIFY_ACCOUNT_BUTTON);
      default:
        break;
    }
  }

  return l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON);
}

std::u16string GetAccessibilityText(bool is_autofill_promo,
                                    SignedInState signed_in_state,
                                    const std::string& email) {
  if (is_autofill_promo && signed_in_state == SignedInState::kWebOnlySignedIn) {
    return l10n_util::GetStringFUTF16(
        IDS_SIGNIN_CONTINUE_AS_BUTTON_ACCESSIBILITY_LABEL,
        {base::UTF8ToUTF16(email)});
  }

  return std::u16string();
}

signin_metrics::PromoAction GetPromoAction(bool is_autofill_promo,
                                           SignedInState signed_in_state) {
  if (is_autofill_promo) {
    switch (signed_in_state) {
      case SignedInState::kSignedOut:
        return signin_metrics::PromoAction::
            PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT;
      case SignedInState::kWebOnlySignedIn:
        return signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT;
      default:
        break;
    }
  }

  return signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
}

}  // namespace

BubbleSignInPromoView::BubbleSignInPromoView(
    Profile* profile,
    BubbleSignInPromoDelegate* delegate,
    signin_metrics::AccessPoint access_point,
    int accounts_promo_message_resource_id,
    ui::ButtonStyle button_style,
    int text_style)
    : delegate_(delegate) {
  DCHECK(!profile->IsGuestSession());
  bool is_autofill_promo = signin::IsAutofillSigninPromo(access_point);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account;
  // Signin promos can be shown in incognito, they use an empty account list.
  if (!profile->IsOffTheRecord()) {
    account = signin_ui_util::GetSingleAccountForPromos(identity_manager);
  }

  const views::LayoutOrientation orientation =
      account.IsEmpty() && !is_autofill_promo
          ? views::LayoutOrientation::kHorizontal
          : views::LayoutOrientation::kVertical;

  std::unique_ptr<views::FlexLayout> layout =
      std::make_unique<views::FlexLayout>();
  layout->SetOrientation(orientation);
  layout->SetDefault(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred, true));
  SetLayoutManager(std::move(layout));

  SignedInState signed_in_state =
      signin_util::GetSignedInState(identity_manager);

  // Set the parameters depending on the signed in state and type of promo.
  int title_resource_id = GetSubtitleID(access_point, signed_in_state,
                                        accounts_promo_message_resource_id);
  std::u16string button_text =
      GetButtonText(is_autofill_promo, signed_in_state, account.given_name);
  std::u16string accessibility_text =
      GetAccessibilityText(is_autofill_promo, signed_in_state, account.email);
  signin_metrics::PromoAction promo_action =
      GetPromoAction(is_autofill_promo, signed_in_state);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (access_point ==
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE) {
    if (extensions::sync_util::IsExtensionsExplicitSigninEnabled()) {
      button_text =
          account.given_name.empty()
              ? l10n_util::GetStringUTF16(IDS_EXTENSIONS_EXPLICIT_SIGNIN_BUTTON)
              : l10n_util::GetStringFUTF16(
                    IDS_EXTENSIONS_EXPLICIT_SIGNIN_BUTTON_WITH_ACCOUNT_AWARENESS,
                    base::UTF8ToUTF16(account.given_name));
    }
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  signin_metrics::LogSignInOffered(access_point, promo_action);

  if (title_resource_id) {
    std::u16string title_text = l10n_util::GetStringUTF16(title_resource_id);
    std::unique_ptr<views::Label> title = std::make_unique<views::Label>(
        title_text, views::style::CONTEXT_DIALOG_BODY_TEXT, text_style);
    title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    title->SetMultiLine(true);
    if (orientation == views::LayoutOrientation::kHorizontal) {
      title->SetMaximumWidth(kTitleMaxWidth);
    } else {
      title->SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(0, 0,
                            ChromeLayoutProvider::Get()
                                ->GetDialogInsetsForContentType(
                                    views::DialogContentType::kText,
                                    views::DialogContentType::kText)
                                .bottom(),
                            0));
    }
    AddChildView(std::move(title));
  }

  views::Button::PressedCallback callback = base::BindRepeating(
      &BubbleSignInPromoView::SignIn, base::Unretained(this));

  std::unique_ptr<BubbleSignInPromoSignInButtonView> signin_button_pointer;

  if (account.IsEmpty()) {
    signin_button_pointer = std::make_unique<BubbleSignInPromoSignInButtonView>(
        std::move(callback), is_autofill_promo, button_style,
        std::move(button_text));

    views::View* button_parent = AddChildView(std::make_unique<views::View>());
    std::unique_ptr<views::FlexLayout> button_layout =
        std::make_unique<views::FlexLayout>();
    button_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
    button_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
    button_parent->SetLayoutManager(std::move(button_layout));
    button_parent->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded));
    signin_button_pointer->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded)
            .WithAlignment(views::LayoutAlignment::kEnd));

    signin_button_view_ =
        button_parent->AddChildView(std::move(signin_button_pointer));
  } else {
    gfx::Image account_icon = account.account_image;
    if (account_icon.IsEmpty()) {
      account_icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          profiles::GetPlaceholderAvatarIconResourceID());
    }
    signin_button_pointer = std::make_unique<BubbleSignInPromoSignInButtonView>(
        account, account_icon, std::move(callback), is_autofill_promo,
        std::move(button_text), std::move(accessibility_text));

    signin_button_view_ = AddChildView(std::move(signin_button_pointer));
  }

  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(access_point);
}

BubbleSignInPromoView::~BubbleSignInPromoView() = default;

void BubbleSignInPromoView::SignIn() {
  std::optional<AccountInfo> account = signin_button_view_->account();
  delegate_->OnSignIn(account.value_or(AccountInfo()));
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
}

BEGIN_METADATA(BubbleSignInPromoView)
END_METADATA
