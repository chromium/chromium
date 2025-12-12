// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_hats_util.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_signin_button_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

using signin_util::SignedInState;

// TODO(crbug.com/391586330): Strings used in this file sometimes originate
// from a different source which makes their name out of context. Look into
// whether it would better to have specific strings for these views.

namespace {

constexpr int kTitleMaxWidth = 218;

int GetSubtitleID(bool is_signin_promo,
                  signin::SignInPromoType promo_type,
                  SignedInState signed_in_state) {
  switch (promo_type) {
    case signin::SignInPromoType::kPassword: {
      switch (signed_in_state) {
        case SignedInState::kSignedOut:
        case SignedInState::kWebOnlySignedIn:
          return IDS_AUTOFILL_SIGNIN_PROMO_SUBTITLE_PASSWORD;
        case SignedInState::kSignInPending:
          return IDS_AUTOFILL_VERIFY_PROMO_SUBTITLE_PASSWORD;
        case SignedInState::kSignedIn:
        case SignedInState::kSyncing:
        case SignedInState::kSyncPaused:
          break;
      }
      break;
      case signin::SignInPromoType::kAddress: {
        switch (signed_in_state) {
          case SignedInState::kSignedOut:
          case SignedInState::kWebOnlySignedIn:
            return IDS_AUTOFILL_SIGNIN_PROMO_SUBTITLE_ADDRESS;
          case SignedInState::kSignInPending:
            return IDS_AUTOFILL_VERIFY_PROMO_SUBTITLE_ADDRESS;
          case SignedInState::kSignedIn:
          case SignedInState::kSyncing:
          case SignedInState::kSyncPaused:
            break;
        }
      } break;
      case signin::SignInPromoType::kBookmark: {
        if (!is_signin_promo) {
          return IDS_BOOKMARK_DICE_PROMO_SYNC_MESSAGE;
        }

        switch (signed_in_state) {
          case SignedInState::kSignedOut:
          case SignedInState::kWebOnlySignedIn:
            return base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)
                       ? IDS_BOOKMARK_INSTALLED_BUBBLE_PROMO_EXPLICIT_SIGNIN_MESSAGE
                       : IDS_BOOKMARK_INSTALLED_PROMO_EXPLICIT_SIGNIN_MESSAGE;
          case SignedInState::kSignInPending:
            return IDS_BOOKMARK_VERIFY_PROMO_SUBTITLE;
          case SignedInState::kSignedIn:
          case SignedInState::kSyncing:
          case SignedInState::kSyncPaused:
            break;
        }
      } break;
      case signin::SignInPromoType::kExtension: {
        if (!is_signin_promo) {
          return IDS_EXTENSION_INSTALLED_DICE_PROMO_SYNC_MESSAGE;
        }

        switch (signed_in_state) {
          case SignedInState::kSignedOut:
          case SignedInState::kWebOnlySignedIn:
            return IDS_EXTENSION_INSTALLED_PROMO_EXPLICIT_SIGNIN_MESSAGE;
          case SignedInState::kSignInPending:
            return IDS_EXTENSION_VERIFY_PROMO_SUBTITLE;
          case SignedInState::kSignedIn:
          case SignedInState::kSyncing:
          case SignedInState::kSyncPaused:
            break;
        }
      }
    }
  }

  NOTREACHED();
}

std::u16string GetButtonText(bool is_signin_promo,
                             SignedInState signed_in_state,
                             const std::string& name) {
  if (is_signin_promo) {
    switch (signed_in_state) {
      case SignedInState::kSignedOut:
      case SignedInState::kWebOnlySignedIn:
        // Note: the name may be empty in `kWebOnlySignedIn`, for example if the
        // current account is not allowed by policy signin pattern.
        return name.empty()
                   ? l10n_util::GetStringUTF16(
                         IDS_PROFILE_MENU_SIGNIN_PROMO_BUTTON)
                   : l10n_util::GetStringFUTF16(
                         IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_ACCEPT_TEXT,
                         {base::UTF8ToUTF16(name)});
      case SignedInState::kSignInPending:
        return l10n_util::GetStringUTF16(IDS_PROFILES_VERIFY_ACCOUNT_BUTTON);
      case SignedInState::kSignedIn:
      case SignedInState::kSyncing:
      case SignedInState::kSyncPaused:
        break;
    }
  }

  return l10n_util::GetStringUTF16(IDS_PROFILES_DICE_SIGNIN_BUTTON);
}

std::u16string GetAccessibilityText(bool is_signin_promo,
                                    SignedInState signed_in_state,
                                    const AccountInfo& account) {
  if (is_signin_promo && signed_in_state == SignedInState::kWebOnlySignedIn &&
      !account.IsEmpty()) {
    return l10n_util::GetStringFUTF16(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_ACCEPT_TEXT,
        {base::UTF8ToUTF16(
            base::StrCat({account.given_name, " ", account.email}))});
  }

  return std::u16string();
}

signin_metrics::PromoAction GetPromoAction(bool is_signin_promo,
                                           SignedInState signed_in_state,
                                           const AccountInfo& account) {
  if (is_signin_promo) {
    switch (signed_in_state) {
      case SignedInState::kSignedOut:
      case SignedInState::kWebOnlySignedIn:
        return account.IsEmpty()
                   ? signin_metrics::PromoAction::
                         PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT
                   : signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT;
      case SignedInState::kSignedIn:
      case SignedInState::kSyncing:
      case SignedInState::kSyncPaused:
      case SignedInState::kSignInPending:
        break;
    }
  }

  return signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
}

void IncrementContextualPromoDismissCountPerSignedOutProfile(
    Profile* profile,
    signin_metrics::AccessPoint access_point) {
  if (!base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)) {
    int dismiss_count = profile->GetPrefs()->GetInteger(
        prefs::kAutofillSignInPromoDismissCountPerProfile);
    profile->GetPrefs()->SetInteger(
        prefs::kAutofillSignInPromoDismissCountPerProfile, dismiss_count + 1);
    return;
  }

  signin::SignInPromoType promo_type =
      signin::GetSignInPromoTypeFromAccessPoint(access_point);
  switch (promo_type) {
    case signin::SignInPromoType::kPassword:
      return profile->GetPrefs()->SetInteger(
          prefs::kPasswordSignInPromoDismissCountPerProfileForLimitsExperiment,
          profile->GetPrefs()->GetInteger(
              prefs::
                  kPasswordSignInPromoDismissCountPerProfileForLimitsExperiment) +
              1);
    case signin::SignInPromoType::kAddress:
      return profile->GetPrefs()->SetInteger(
          prefs::kAddressSignInPromoDismissCountPerProfileForLimitsExperiment,
          profile->GetPrefs()->GetInteger(
              prefs::
                  kAddressSignInPromoDismissCountPerProfileForLimitsExperiment) +
              1);
    case signin::SignInPromoType::kBookmark:
      CHECK(base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));
      return profile->GetPrefs()->SetInteger(
          prefs::kBookmarkSignInPromoDismissCountPerProfileForLimitsExperiment,
          profile->GetPrefs()->GetInteger(
              prefs::
                  kBookmarkSignInPromoDismissCountPerProfileForLimitsExperiment) +
              1);
    case signin::SignInPromoType::kExtension:
      NOTREACHED();
  }
}

void IncrementContextualPromoDismissCountPerAccount(
    Profile* profile,
    signin_metrics::AccessPoint access_point,
    const AccountInfo& account) {
  if (!base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)) {
    SigninPrefs(*profile->GetPrefs())
        .IncrementAutofillSigninPromoDismissCount(account.gaia);
    return;
  }

  signin::SignInPromoType promo_type =
      signin::GetSignInPromoTypeFromAccessPoint(access_point);
  switch (promo_type) {
    case signin::SignInPromoType::kPassword:
      SigninPrefs(*profile->GetPrefs())
          .IncrementPasswordSigninPromoDismissCount(account.gaia);
      break;
    case signin::SignInPromoType::kAddress:
      SigninPrefs(*profile->GetPrefs())
          .IncrementAddressSigninPromoDismissCount(account.gaia);
      break;
    case signin::SignInPromoType::kBookmark:
      CHECK(base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp));
      SigninPrefs(*profile->GetPrefs())
          .IncrementBookmarkSigninPromoDismissCount(account.gaia);
      break;
    case signin::SignInPromoType::kExtension:
      NOTREACHED();
  }
}

}  // namespace

BubbleSignInPromoView::BubbleSignInPromoView(
    content::WebContents* web_contents,
    signin_metrics::AccessPoint access_point,
    syncer::LocalDataItemModel::DataId data_id,
    ui::ButtonStyle button_style)
    : access_point_(access_point),
      delegate_(
          std::make_unique<BubbleSignInPromoDelegate>(*web_contents,
                                                      access_point,
                                                      std::move(data_id))) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetOriginalProfile();
  DCHECK(!profile->IsGuestSession());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin::SignInPromoType promo_type =
      signin::GetSignInPromoTypeFromAccessPoint(access_point);
  SignedInState signed_in_state =
      signin_util::GetSignedInState(identity_manager);
  bool is_signin_promo = signin::IsSignInPromo(access_point);

  AccountInfo account;
  // Sync promos can be shown in incognito, they use an empty account list.
  if (!Profile::FromBrowserContext(web_contents->GetBrowserContext())
           ->IsOffTheRecord()) {
    account = signin_ui_util::GetSingleAccountForPromos(identity_manager);
  }

  // Set the layout.
  const views::LayoutOrientation orientation =
      account.IsEmpty() && !is_signin_promo
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

  // Set the parameters depending on the signed in state and type of promo.
  int title_resource_id =
      GetSubtitleID(is_signin_promo, promo_type, signed_in_state);
  std::u16string button_text =
      GetButtonText(is_signin_promo, signed_in_state, account.given_name);
  std::u16string accessibility_text =
      GetAccessibilityText(is_signin_promo, signed_in_state, account);
  signin_metrics::PromoAction promo_action =
      GetPromoAction(is_signin_promo, signed_in_state, account);

  // Set subtitle.
  std::u16string title_text = l10n_util::GetStringUTF16(title_resource_id);
  std::unique_ptr<views::Label> title = std::make_unique<views::Label>(
      title_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY);
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title->SetMultiLine(true);
  if (orientation == views::LayoutOrientation::kHorizontal) {
    title->SetMaximumWidth(kTitleMaxWidth);
  } else {
    // Make the distance smaller if the next element will be an account card.
    const int subtitle_margin_bottom =
        account.IsEmpty() ? ChromeLayoutProvider::Get()
                                ->GetDialogInsetsForContentType(
                                    views::DialogContentType::kText,
                                    views::DialogContentType::kText)
                                .bottom()
                          : ChromeLayoutProvider::Get()->GetDistanceMetric(
                                DISTANCE_TEXTFIELD_ACCOUNT_CARD_VERTICAL);
    title->SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(0, 0, subtitle_margin_bottom, 0));
  }
  AddChildView(std::move(title));

  // Create button with callback.
  std::unique_ptr<BubbleSignInPromoSignInButtonView> signin_button_pointer;
  views::Button::PressedCallback callback = base::BindRepeating(
      &BubbleSignInPromoView::SignIn, base::Unretained(this));

  if (account.IsEmpty()) {
    signin_button_pointer = std::make_unique<BubbleSignInPromoSignInButtonView>(
        std::move(callback), access_point, button_style,
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
        account, account_icon, std::move(callback), access_point,
        std::move(button_text), std::move(accessibility_text));

    signin_button_view_ = AddChildView(std::move(signin_button_pointer));
  }

  // Record metrics and prefs.
  if (promo_action !=
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO) {
    signin_metrics::LogSignInOffered(access_point, promo_action);
  }
  if (signed_in_state == signin_util::SignedInState::kSignInPending) {
    signin_metrics::LogSigninPendingOffered(access_point);
  }

  signin_metrics::RecordSigninImpressionUserActionForAccessPoint(access_point);
  signin::RecordSignInPromoShown(access_point, profile);
}

BubbleSignInPromoView::~BubbleSignInPromoView() = default;

views::View* BubbleSignInPromoView::GetSignInButton() const {
  return signin_button_view_ ? signin_button_view_->GetSignInButton() : nullptr;
}

void BubbleSignInPromoView::SignIn() {
  std::optional<AccountInfo> account = signin_button_view_->account();
  delegate_->OnSignIn(account.value_or(AccountInfo()));
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
}

void BubbleSignInPromoView::AddedToWidget() {
  if (signin::IsBubbleSigninPromo(access_point_)) {
    scoped_widget_observation_.Observe(GetWidget());
  }
}

void BubbleSignInPromoView::OnWidgetDestroying(views::Widget* widget) {
  // This should only be recorded for autofill bubble promos. Not for those
  // displayed in another bubble's footer.
  if (!signin::IsBubbleSigninPromo(access_point_)) {
    return;
  }

  scoped_widget_observation_.Reset();
  std::string dismiss_action;

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kCloseButtonClicked:
      dismiss_action = "CloseButton";
      break;
    case views::Widget::ClosedReason::kEscKeyPressed:
      dismiss_action = "EscapeKey";
      break;
    case views::Widget::ClosedReason::kUnspecified:
    case views::Widget::ClosedReason::kLostFocus:
    case views::Widget::ClosedReason::kCancelButtonClicked:
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      // Don't record anything if the bubble was not actively dismissed by the
      // user.
      return;
  }

  CHECK(!dismiss_action.empty());

  Profile* profile = Profile::FromBrowserContext(
      delegate_->GetWebContents()->GetBrowserContext());
  AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(profile));

  // Count the number of times the promo was dismissed in order to not show it
  // anymore after 2 dismissals.
  if (account.gaia.empty()) {
    IncrementContextualPromoDismissCountPerSignedOutProfile(profile,
                                                            access_point_);
  } else {
    IncrementContextualPromoDismissCountPerAccount(profile, access_point_,
                                                   account);
  }

  // Launch a HaTS survey if the user actively dismissed the promo.
  signin::LaunchSigninHatsSurveyForProfile(
      kHatsSurveyTriggerIdentitySigninPromoBubbleDismissed, profile,
      /*defer_if_no_browser=*/false, access_point_);

  base::UmaHistogramEnumeration(
      base::StrCat({"Signin.SignInPromo.Dismissed", dismiss_action}),
      access_point_);
}

BEGIN_METADATA(BubbleSignInPromoView)
END_METADATA
