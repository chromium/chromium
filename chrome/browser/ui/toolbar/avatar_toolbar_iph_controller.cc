// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/avatar_toolbar_iph_controller.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/common/url_utils.h"

namespace {

base::TimeDelta g_iph_min_delay_after_creation = base::Seconds(2);

}  // namespace

DEFINE_USER_DATA(AvatarToolbarIphController);

AvatarToolbarIphController::AvatarToolbarIphController(
    BrowserWindowInterface& browser,
    Profile& profile,
    signin::IdentityManager* identity_manager)
    : browser_(browser),
      profile_(profile),
      identity_manager_(identity_manager),
      creation_time_(base::TimeTicks::Now()),
      scoped_unowned_user_data_(browser.GetUnownedUserDataHost(), *this) {}

AvatarToolbarIphController::~AvatarToolbarIphController() = default;

// static
AvatarToolbarIphController* AvatarToolbarIphController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

bool AvatarToolbarIphController::ShouldShowAvatarToolbarIPH() {
  if (profile_->IsGuestSession() || profile_->IsIncognitoProfile() ||
      profile_->IsEnterpriseIsolatedModeProfile()) {
    return false;
  }
  auto* provider = ToolbarButtonProvider::From(&browser_.get());
  auto* avatar =
      provider ? provider->GetAvatarToolbarButtonInterface() : nullptr;
  return avatar && avatar->IsReadyForIPH();
}

void AvatarToolbarIphController::MaybeShowProfileSwitchIPH() {
  MaybeShowProfileSwitchIPHImpl();
}

void AvatarToolbarIphController::MaybeShowProfileSwitchIPHImpl() {
  if (!ShouldShowAvatarToolbarIPH()) {
    return;
  }

  // Wait a small delay after controller creation for a smoother animation.
  base::TimeDelta time_since_creation = base::TimeTicks::Now() - creation_time_;
  if (time_since_creation < g_iph_min_delay_after_creation) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &AvatarToolbarIphController::MaybeShowProfileSwitchIPHImpl,
            weak_ptr_factory_.GetWeakPtr()),
        g_iph_min_delay_after_creation - time_since_creation);
    return;
  }

  // This will show the promo only after the IPH system is properly initialized.
  if (!web_app::AppBrowserController::IsWebApp(&browser_.get())) {
    BrowserUserEducationInterface::From(&browser_.get())
        ->MaybeShowStartupFeaturePromo(
            feature_engagement::kIPHProfileSwitchFeature);
  } else {
    // Installable PasswordManager WebUI is the only web app that has an avatar
    // toolbar button.
    auto app_url =
        web_app::AppBrowserController::From(&browser_.get())->GetAppStartUrl();
    CHECK(
        content::HasWebUIScheme(app_url) &&
        (app_url.GetHost() == password_manager::kChromeUIPasswordManagerHost));
    BrowserUserEducationInterface::From(&browser_.get())
        ->MaybeShowStartupFeaturePromo(
            feature_engagement::kIPHPasswordsWebAppProfileSwitchFeature);
  }
}

void AvatarToolbarIphController::MaybeShowSupervisedUserProfileSignInIPH() {
  MaybeShowSupervisedUserProfileSignInIPHImpl();
}

void AvatarToolbarIphController::MaybeShowSupervisedUserProfileSignInIPHImpl() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (!ShouldShowAvatarToolbarIPH()) {
    return;
  }
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHSupervisedUserProfileSigninFeature)) {
    return;
  }
  CHECK(identity_manager_);
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }

  auto account_info = identity_manager_->FindExtendedAccountInfoByAccountId(
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  if (account_info.GetAccountCapabilities().is_subject_to_parental_controls() !=
      signin::Tribool::kTrue) {
    return;
  }
  if (account_info.IsEmpty()) {
    return;
  }

  // This delay also gives the anchor element time to become visible.
  // TODO(crbug.com/372689164): Investigate alternative rescheduling using
  // `WouldShowFeaturePromo`.
  base::TimeDelta time_since_creation = base::TimeTicks::Now() - creation_time_;
  if (time_since_creation < g_iph_min_delay_after_creation) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AvatarToolbarIphController::
                           MaybeShowSupervisedUserProfileSignInIPHImpl,
                       weak_ptr_factory_.GetWeakPtr()),
        g_iph_min_delay_after_creation - time_since_creation);
    return;
  }

  user_education::FeaturePromoParams params(
      feature_engagement::kIPHSupervisedUserProfileSigninFeature);
  params.title_params =
      base::UTF8ToUTF16(account_info.GetGivenName().value_or(""));
  BrowserUserEducationInterface::From(&browser_.get())
      ->MaybeShowFeaturePromo(std::move(params));
#endif
}

// static
base::AutoReset<base::TimeDelta>
AvatarToolbarIphController::SetScopedIPHMinDelayAfterCreationForTesting(
    base::TimeDelta delay) {
  return base::AutoReset<base::TimeDelta>(&g_iph_min_delay_after_creation,
                                          delay);
}
