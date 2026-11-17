// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/incognito_menu_view.h"
#include "chrome/browser/ui/views/profiles/isolated_mode_menu_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

ProfileMenuCoordinator::~ProfileMenuCoordinator() {
  // Ensure the ProfileMenuCoordinator does not outlive its associated bubble
  // widget to mitigate the risk of dangling references.
  if (bubble_tracker_ && bubble_tracker_.view()->GetWidget()) {
    bubble_tracker_.view()->GetWidget()->CloseNow();
  }
}

void ProfileMenuCoordinator::Show(bool is_source_accelerator,
                                  bool from_avatar_promo) {
  // Do not show avatar bubble if the avatar button cannot be shown for this
  // profile, or if the bubble is already showing.
  if (!AvatarToolbarButtonInterface::CanShowForProfile(GetProfile()) ||
      IsShowing()) {
    return;
  }

  // Do not show avatar bubble if there is no avatar menu button in this window.
  // `BrowserView` windows register a `ToolbarButtonProvider`, whose avatar
  // button interface is null in windows that do not construct an avatar toolbar
  // button (such as popups or most web apps). Experimental `WebUIBrowser`
  // windows do not register a `ToolbarButtonProvider` and instead expose the
  // avatar button directly via `BrowserElements`.
  if (ToolbarButtonProvider* toolbar_button_provider =
          ToolbarButtonProvider::From(GetBrowser())) {
    if (!toolbar_button_provider->GetAvatarToolbarButtonInterface()) {
      return;
    }
  } else if (!BrowserElements::From(GetBrowser())
                  ->GetElement(kToolbarAvatarButtonElementId)) {
    return;
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Batch upload promos in the profile menu identity header are only shown
  // if the user opened the menu via the avatar promo pill. Outside of pill
  // expansions, batch upload promos are suppressed so they don't overshadow
  // other promos (such as History Sync), while still fetching local data count
  // to show the batch upload row item in the menu.
  const bool allow_batch_upload_promos = from_avatar_promo;

  signin::ComputeProfileMenuAvatarButtonPromoInfo(
      *GetProfile(),
      base::BindOnce(&ProfileMenuCoordinator::ShowWithPromoResults,
                     weak_pointer_factory_.GetWeakPtr(), is_source_accelerator,
                     from_avatar_promo),
      allow_batch_upload_promos);
#else
  ShowWithPromoResults(is_source_accelerator, from_avatar_promo);
#endif
}

void ProfileMenuCoordinator::ShowWithPromoResults(
    bool is_source_accelerator,
    bool from_avatar_promo
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    ,
    signin::ProfileMenuAvatarButtonPromoInfo promo_info
#endif
) {
  // Results are asynchronous, which can cause the menu to be already shown
  // before receiving them. If this happens, ignore the second request as the
  // menu is already shown.
  if (IsShowing()) {
    return;
  }

  if (ToolbarButtonProvider* toolbar_button_provider =
          ToolbarButtonProvider::From(GetBrowser())) {
    AvatarToolbarButtonInterface* avatar_button =
        toolbar_button_provider->GetAvatarToolbarButtonInterface();
    if (!avatar_button) {
      return;
    }
    avatar_button->RunWhenVisible(base::BindOnce(
        &ProfileMenuCoordinator::ShowBubble, weak_pointer_factory_.GetWeakPtr(),
        is_source_accelerator, from_avatar_promo
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
        ,
        std::move(promo_info)
#endif
            ));
    return;
  }

  // Fallback for windows without a `ToolbarButtonProvider` (such as
  // experimental `WebUIBrowser` windows), which expose the avatar button
  // directly via `BrowserElements`.
  ShowBubble(
      is_source_accelerator, from_avatar_promo,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      std::move(promo_info),
#endif
      views::BubbleAnchor(BrowserElements::From(GetBrowser())
                              ->GetElement(kToolbarAvatarButtonElementId)),
      base::ScopedClosureRunner());
}

void ProfileMenuCoordinator::ShowBubble(
    bool is_source_accelerator,
    bool from_avatar_promo,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    signin::ProfileMenuAvatarButtonPromoInfo promo_info,
#endif
    views::BubbleAnchor anchor,
    base::ScopedClosureRunner keep_anchor_visible) {
  // Check again in case `RunWhenVisible()` resolved asynchronously, and
  // bail out if `RunWhenVisible()` fell back to the parent container view
  // because the avatar button itself stayed hidden (such as single-profile
  // macOS web apps).
  if (IsShowing() || anchor.IsNull() ||
      (anchor.GetIfView() &&
       !views::IsViewClass<AvatarToolbarButton>(anchor.GetIfView()))) {
    return;
  }

  signin_ui_util::RecordProfileMenuViewShown(GetProfile());
  // Close any existing IPH bubble for the profile menu.
  BrowserUserEducationInterface::From(GetBrowser())
      ->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHProfileSwitchFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  BrowserUserEducationInterface::From(GetBrowser())
      ->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHSupervisedUserProfileSigninFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
#endif

  std::unique_ptr<ProfileMenuViewBase> bubble;
  const bool is_incognito = GetProfile()->IsIncognitoProfile();
  if (is_incognito) {
    bubble = std::make_unique<IncognitoMenuView>(anchor, &browser_.get());
  } else if (GetProfile()->IsEnterpriseIsolatedModeProfile()) {
    bubble = std::make_unique<IsolatedModeMenuView>(anchor, &browser_.get());
  } else {
#if BUILDFLAG(IS_CHROMEOS)
    // Note: on Ash, only incognito windows have a profile menu.
    NOTREACHED() << "The profile menu is not implemented on Ash.";
#else
    bubble = std::make_unique<ProfileMenuView>(anchor, &browser_.get(),
                                               promo_info, from_avatar_promo);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
  bubble->SetProperty(views::kElementIdentifierKey,
                      kToolbarAvatarBubbleElementId);

  auto* bubble_ptr = bubble.get();
  CHECK_EQ(nullptr, bubble_tracker_.view(), base::NotFatalUntil::M162);
  bubble_tracker_.SetView(bubble_ptr);
  // Keep the avatar button from being hidden until the bubble is destroyed.
  bubble_tracker_.SetIsDeletingCallback(
      base::DoNothingWithBoundArgs(std::move(keep_anchor_visible)));

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  bubble_ptr->CreateAXWidgetObserver(widget);
  widget->Show();
  if (is_source_accelerator) {
    bubble_ptr->FocusFirstProfileButton();
  }
}

bool ProfileMenuCoordinator::IsShowing() const {
  return bubble_tracker_.view() != nullptr;
}

ProfileMenuViewBase*
ProfileMenuCoordinator::GetProfileMenuViewBaseForTesting() {
  return IsShowing()
             ? views::AsViewClass<ProfileMenuViewBase>(bubble_tracker_.view())
             : nullptr;
}

BrowserWindowInterface* ProfileMenuCoordinator::GetBrowser() {
  return &browser_.get();
}

Profile* ProfileMenuCoordinator::GetProfile() {
  return &profile_.get();
}

DEFINE_USER_DATA(ProfileMenuCoordinator);

// static
ProfileMenuCoordinator* ProfileMenuCoordinator::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

ProfileMenuCoordinator::ProfileMenuCoordinator(BrowserWindowInterface* browser,
                                               Profile* profile)
    : scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this),
      browser_(CHECK_DEREF(browser)),
      profile_(CHECK_DEREF(profile)) {}
