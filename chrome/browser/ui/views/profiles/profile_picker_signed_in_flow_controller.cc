// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"

#include "base/strings/string_util.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_turn_sync_on_delegate.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/window_open_disposition.h"

namespace {
// Returns  true if the url is a url that should be opened after a browser is
// created following a profile profile creation. Now only the two factor
// intersitial url is supported since that url is bypassed while in the profile
// picker.
bool ShouldOpenUrlAfterBrowserCreation(const GURL& url) {
  return base::StartsWith(url.spec(), chrome::kGoogleTwoFactorIntersitialURL);
}

// Opens a new tab with `url` in `browser`. Tabs opened using this function will
// not replace existing tabs.
void OpenNewTabInBrowser(const GURL& url, Browser* browser) {
  if (browser) {
    browser->OpenGURL(url, WindowOpenDisposition::SINGLETON_TAB);
  }
}

}  //  namespace

ProfilePickerSignedInFlowController::ProfilePickerSignedInFlowController(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    const CoreAccountInfo& account_info,
    std::unique_ptr<content::WebContents> contents,
    signin_metrics::AccessPoint signin_access_point,
    std::optional<SkColor> profile_color)
    : host_(host),
      profile_(profile),
      account_info_(account_info),
      contents_(std::move(contents)),
      signin_access_point_(signin_access_point),
      profile_color_(profile_color) {
  DCHECK(profile_);
  DCHECK(contents_);
  // TODO(crbug.com/40216113): Consider renaming the enum entry -- this does not
  // have to be profile creation flow, it can be profile onboarding.
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kProfileCreationFlow);
  if (ShouldOpenUrlAfterBrowserCreation(contents_->GetVisibleURL())) {
    url_to_open_ = contents_->GetVisibleURL();
  }
}

ProfilePickerSignedInFlowController::~ProfilePickerSignedInFlowController() {
  if (contents()) {
    contents()->SetDelegate(nullptr);
  }
}

void ProfilePickerSignedInFlowController::Init() {
  DCHECK(!IsInitialized());

  contents()->SetDelegate(this);

  const CoreAccountInfo& account_info =
      IdentityManagerFactory::GetForProfile(profile_)->FindExtendedAccountInfo(
          account_info_);
  DCHECK(!account_info.IsEmpty())
      << "A profile with a valid account must be passed in.";
  email_ = account_info.email;

  base::OnceClosure sync_consent_completed_closure =
      base::BindOnce(&ProfilePickerSignedInFlowController::FinishAndOpenBrowser,
                     weak_ptr_factory_.GetWeakPtr(), PostHostClearedCallback());

  // TurnSyncOnHelper deletes itself once done.
  new TurnSyncOnHelper(
      profile_, signin_access_point_,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      account_info.account_id,
      TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
      std::make_unique<ProfilePickerTurnSyncOnDelegate>(
          weak_ptr_factory_.GetWeakPtr(), profile_),
      std::move(sync_consent_completed_closure));
}

void ProfilePickerSignedInFlowController::Cancel() {}

void ProfilePickerSignedInFlowController::FinishAndOpenBrowser(
    PostHostClearedCallback callback) {
  bool is_continue_callback = !callback->is_null();
  if (url_to_open_.is_valid()) {
    auto open_url_callback = PostHostClearedCallback(
        base::BindOnce(&OpenNewTabInBrowser, url_to_open_));
    callback = is_continue_callback
                   ? PostHostClearedCallback(base::BindOnce(
                         [](PostHostClearedCallback cb1,
                            PostHostClearedCallback cb2, Browser* browser) {
                           std::move(*cb1).Run(browser);
                           std::move(*cb2).Run(browser);
                         },
                         std::move(open_url_callback), std::move(callback)))
                   : PostHostClearedCallback(std::move(open_url_callback));
  }

  FinishAndOpenBrowserInternal(std::move(callback), is_continue_callback);
}

void ProfilePickerSignedInFlowController::SwitchToSyncConfirmation() {
  DCHECK(IsInitialized());
  host_->ShowScreen(contents(), GetSyncConfirmationURL(/*loading=*/false),
                    /*navigation_finished_closure=*/
                    base::BindOnce(&ProfilePickerSignedInFlowController::
                                       SwitchToSyncConfirmationFinished,
                                   // Unretained is enough as the callback is
                                   // called by the owner of this instance.
                                   base::Unretained(this)));
}

void ProfilePickerSignedInFlowController::SwitchToManagedUserProfileNotice(
    ManagedUserProfileNoticeUI::ScreenType type,
    signin::SigninChoiceCallback process_user_choice_callback) {
  DCHECK(IsInitialized());
  host_->ShowScreen(contents(),
                    GURL(chrome::kChromeUIManagedUserProfileNoticeUrl),
                    /*navigation_finished_closure=*/
                    base::BindOnce(&ProfilePickerSignedInFlowController::
                                       SwitchToManagedUserProfileNoticeFinished,
                                   // Unretained is enough as the callback is
                                   // called by the owner of this instance.
                                   base::Unretained(this), type,
                                   std::move(process_user_choice_callback)));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ProfilePickerSignedInFlowController::SwitchToLacrosIntro(
    signin::SigninChoiceCallback proceed_callback) {
  NOTREACHED_IN_MIGRATION();
}
#endif

void ProfilePickerSignedInFlowController::SwitchToProfileSwitch(
    const base::FilePath& profile_path) {
  DCHECK(IsInitialized());
  // The sign-in flow is finished, no profile window should be shown in the end.
  Cancel();

  switch_profile_path_ = profile_path;
  host_->ShowScreenInPickerContents(
      GURL(chrome::kChromeUIProfilePickerUrl).Resolve("profile-switch"));
}

void ProfilePickerSignedInFlowController::ResetHostAndShowErrorDialog(
    const ForceSigninUIError& error) {
  CHECK(IsInitialized());

  Cancel();
  host_->Reset(
      base::BindOnce(&ProfilePickerWebContentsHost::ShowForceSigninErrorDialog,
                     base::Unretained(host_), error));
}

std::optional<SkColor> ProfilePickerSignedInFlowController::GetProfileColor()
    const {
  // The new profile theme may be overridden by an existing policy theme. This
  // check ensures the correct theme is applied to the sync confirmation window.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service->UsingPolicyTheme()) {
    return theme_service->GetPolicyThemeColor();
  }
  return profile_color_;
}

GURL ProfilePickerSignedInFlowController::GetSyncConfirmationURL(bool loading) {
  GURL url = GURL(chrome::kChromeUISyncConfirmationURL);
  return AppendSyncConfirmationQueryParams(
      loading ? url.Resolve(chrome::kChromeUISyncConfirmationLoadingPath) : url,
      SyncConfirmationStyle::kWindow, /*is_sync_promo=*/true);
}

std::unique_ptr<content::WebContents>
ProfilePickerSignedInFlowController::ReleaseContents() {
  return std::move(contents_);
}

bool ProfilePickerSignedInFlowController::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

bool ProfilePickerSignedInFlowController::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return host_->GetWebContentsDelegate()->HandleKeyboardEvent(source, event);
}

void ProfilePickerSignedInFlowController::SwitchToSyncConfirmationFinished() {
  DCHECK(IsInitialized());
  // Initialize the WebUI page once we know it's committed.
  SyncConfirmationUI* sync_confirmation_ui =
      static_cast<SyncConfirmationUI*>(contents()->GetWebUI()->GetController());

  sync_confirmation_ui->InitializeMessageHandlerWithBrowser(nullptr);
}

void ProfilePickerSignedInFlowController::
    SwitchToManagedUserProfileNoticeFinished(
        ManagedUserProfileNoticeUI::ScreenType type,
        signin::SigninChoiceCallback process_user_choice_callback) {
  DCHECK(IsInitialized());
  // Initialize the WebUI page once we know it's committed.
  ManagedUserProfileNoticeUI* managed_user_profile_notice_ui =
      contents()
          ->GetWebUI()
          ->GetController()
          ->GetAs<ManagedUserProfileNoticeUI>();

  // Here `done_callback` does nothing because lifecycle of
  // `managed_user_profile_notice_ui` is controlled by this class.
  managed_user_profile_notice_ui->Initialize(
      /*browser=*/nullptr, type,
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          IdentityManagerFactory::GetForProfile(profile_)
              ->FindExtendedAccountInfoByEmailAddress(email_),
          /*is_oidc_account=*/type ==
              ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC,
          /*profile_creation_required_by_policy=*/false,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          std::move(process_user_choice_callback),
          /*done_callback=*/base::OnceClosure()));
}

bool ProfilePickerSignedInFlowController::IsInitialized() const {
  // `email_` is set in Init(), use it as the proxy here.
  return !email_.empty();
}
