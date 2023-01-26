// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_dice_sign_in_provider.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_encryption_keys_tab_helper.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"

namespace {

GURL GetSigninURL(bool dark_mode) {
  GURL signin_url = GaiaUrls::GetInstance()->signin_chrome_sync_dice();
  if (dark_mode)
    signin_url = net::AppendQueryParameter(signin_url, "color_scheme", "dark");
  return signin_url;
}

bool IsExternalURL(const GURL& url) {
  // Empty URL is used initially, about:blank is used to stop navigation after
  // sign-in succeeds.
  if (url.is_empty() || url == GURL(url::kAboutBlankURL))
    return false;
  if (gaia::HasGaiaSchemeHostPort(url))
    return false;
  return true;
}

}  // namespace

ProfilePickerDiceSignInProvider::ProfilePickerDiceSignInProvider(
    ProfilePickerWebContentsHost* host,
    signin_metrics::AccessPoint signin_access_point,
    absl::optional<base::FilePath> profile_path)
    : host_(host),
      signin_access_point_(signin_access_point),
      profile_path_(profile_path) {
  // If the path is provided, it must be non-empty.
  DCHECK(!(profile_path.has_value() && profile_path->empty()));
}

ProfilePickerDiceSignInProvider::~ProfilePickerDiceSignInProvider() {
  // Handle unfinished signed-in profile creation (i.e. when callback was not
  // called yet).
  if (callback_) {
    if (IsInitialized()) {
      contents()->SetDelegate(nullptr);

      // Schedule the ephemeral profile for deletion if it wasn't deleted yet,
      // since it's not needed any more.
      if (!profile_path_.has_value() &&
          !IsProfileDirectoryMarkedForDeletion(profile_->GetPath())) {
        g_browser_process->profile_manager()
            ->GetDeleteProfileHelper()
            .ScheduleEphemeralProfileForDeletion(profile_->GetPath());
      }
    }

    ProfileMetrics::LogProfileAddSignInFlowOutcome(
        ProfileMetrics::ProfileSignedInFlowOutcome::kAbortedBeforeSignIn);
  }
}

void ProfilePickerDiceSignInProvider::SwitchToSignIn(
    base::OnceCallback<void(bool)> switch_finished_callback,
    SignedInCallback signin_finished_callback) {
  // Update the callback even if the profile is already initialized (to respect
  // that the callback may be different).
  callback_ = std::move(signin_finished_callback);

  if (IsInitialized()) {
    std::move(switch_finished_callback).Run(true);
    // Do not load any url because the desired sign-in screen is still loaded in
    // `contents()`.
    host_->ShowScreen(contents(), GURL());
    host_->SetNativeToolbarVisible(true);
    return;
  }

  auto profile_init_callback = base::BindOnce(
      &ProfilePickerDiceSignInProvider::OnProfileInitialized,
      weak_ptr_factory_.GetWeakPtr(), std::move(switch_finished_callback));
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_path_.has_value()) {
    bool profile_exists = profile_manager->LoadProfileByPath(
        profile_path_.value(), /*incognito=*/false,
        std::move(profile_init_callback));
    DCHECK(profile_exists);
  } else {
    size_t icon_index = profiles::GetPlaceholderAvatarIndex();
    // Silently create the new profile for browsing on GAIA (so that the sign-in
    // cookies are stored in the right profile).
    ProfileManager::CreateMultiProfileAsync(
        profile_manager->GetProfileAttributesStorage().ChooseNameForNewProfile(
            icon_index),
        icon_index, /*is_hidden=*/true, std::move(profile_init_callback));
  }
}

void ProfilePickerDiceSignInProvider::ReloadSignInPage() {
  if (IsInitialized() && contents()) {
    contents()->GetController().Reload(content::ReloadType::BYPASSING_CACHE,
                                       true);
  }
}

void ProfilePickerDiceSignInProvider::NavigateBack() {
  if (!IsInitialized() || !contents())
    return;

  if (contents()->GetController().CanGoBack()) {
    contents()->GetController().GoBack();
    return;
  }

  // Move from sign-in back to the previous screen of profile creation.
  // Do not load any url because the desired screen is still loaded in the
  // picker contents.
  host_->ShowScreenInPickerContents(GURL());
  host_->SetNativeToolbarVisible(false);
}

bool ProfilePickerDiceSignInProvider::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

void ProfilePickerDiceSignInProvider::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  NavigateParams params(profile_, target_url, ui::PAGE_TRANSITION_LINK);
  // Open all links as new popups.
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.contents_to_insert = std::move(new_contents);

  params.window_bounds = window_features.bounds;
  Navigate(&params);
}

bool ProfilePickerDiceSignInProvider::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return host_->GetWebContentsDelegate()->HandleKeyboardEvent(source, event);
}

void ProfilePickerDiceSignInProvider::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (source == contents_.get() && IsExternalURL(contents_->GetVisibleURL())) {
    // Attach DiceTabHelper to `contents_` so that sync consent dialog appears
    // after a successful sign-in.
    DiceTabHelper::CreateForWebContents(contents_.get());
    DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(contents_.get());
    // Use |redirect_url| and not |continue_url|, so that the DiceTabHelper can
    // redirect to chrome:// URLs such as the NTP.
    tab_helper->InitializeSigninFlow(
        GetSigninURL(host_->ShouldUseDarkColors()), signin_access_point_,
        signin_metrics::Reason::kSigninPrimaryAccount,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
        GURL(chrome::kChromeUINewTabURL));

    // The rest of the SAML flow logic is handled by the signed-in flow
    // controller.
    FinishFlow(/*is_saml=*/true);
  }
}

web_modal::WebContentsModalDialogHost*
ProfilePickerDiceSignInProvider::GetWebContentsModalDialogHost() {
  return host_->GetWebContentsModalDialogHost();
}

void ProfilePickerDiceSignInProvider::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK(IsInitialized());
  refresh_token_updated_ = true;
  FinishFlowIfSignedIn();
}

void ProfilePickerDiceSignInProvider::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  DCHECK(IsInitialized());
  FinishFlowIfSignedIn();
}

void ProfilePickerDiceSignInProvider::FinishFlowIfSignedIn() {
  DCHECK(IsInitialized());

  if (IdentityManagerFactory::GetForProfile(profile_)
          ->HasPrimaryAccountWithRefreshToken(signin::ConsentLevel::kSignin) &&
      refresh_token_updated_) {
    FinishFlow(/*is_saml=*/false);
  }
}

void ProfilePickerDiceSignInProvider::OnProfileInitialized(
    base::OnceCallback<void(bool)> switch_finished_callback,
    Profile* new_profile) {
  if (!new_profile) {
    std::move(switch_finished_callback).Run(false);
    return;
  }
  DCHECK(!profile_);
  DCHECK(!contents());

  profile_ = new_profile;
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kProfileCreationFlow);

  contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  contents()->SetDelegate(this);

  // Create a manager that supports modal dialogs, such as for webauthn.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(contents());
  web_modal::WebContentsModalDialogManager::FromWebContents(contents())
      ->SetDelegate(this);

  // To allow passing encryption keys during interactions with the page,
  // instantiate SyncEncryptionKeysTabHelper.
  SyncEncryptionKeysTabHelper::CreateForWebContents(contents());

  // Listen for sign-in getting completed.
  identity_manager_observation_.Observe(
      IdentityManagerFactory::GetForProfile(profile_));

  // Record that the sign in process starts (its end is recorded automatically
  // by the instance of DiceTurnSyncOnHelper constructed later on in
  // ProfilePickerSignedInFlowController).
  signin_metrics::RecordSigninUserActionForAccessPoint(signin_access_point_);
  signin_metrics::LogSigninAccessPointStarted(
      signin_access_point_,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);

  // Apply the default theme to get consistent colors for toolbars in newly
  // created profiles (this matters for linux where the 'system' theme is used
  // for new profiles).
  if (!profile_path_.has_value()) {
    auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
    theme_service->UseDefaultTheme();
  }

  // Make sure the web contents used for sign-in has proper background to match
  // the toolbar (for dark mode).
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      contents(), host_->GetPreferredBackgroundColor());

  base::OnceClosure navigation_finished_closure =
      base::BindOnce(&ProfilePickerWebContentsHost::SetNativeToolbarVisible,
                     // Unretained is enough as the callback is called by the
                     // host itself.
                     base::Unretained(host_), /*visible=*/true)
          .Then(base::BindOnce(std::move(switch_finished_callback), true));
  host_->ShowScreen(contents(), GetSigninURL(host_->ShouldUseDarkColors()),
                    std::move(navigation_finished_closure));
}

bool ProfilePickerDiceSignInProvider::IsInitialized() const {
  return profile_ != nullptr;
}

void ProfilePickerDiceSignInProvider::FinishFlow(bool is_saml) {
  DCHECK(IsInitialized());
  host_->SetNativeToolbarVisible(false);
  contents()->SetDelegate(nullptr);
  identity_manager_observation_.Reset();
  std::move(callback_).Run(profile_.get(), is_saml, std::move(contents_));
}
