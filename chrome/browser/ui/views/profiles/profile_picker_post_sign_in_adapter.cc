// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_post_sign_in_adapter.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/features.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/url_util.h"
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

// Returns the URL for history sync optin screen.
GURL GetHistorySyncOptinURL() {
  return HistorySyncOptinUI::AppendHistorySyncOptinQueryParams(
      GURL(chrome::kChromeUIHistorySyncOptinURL),
      HistorySyncOptinLaunchContext::kWindow);
}

void OnManagementUserChoice(signin::SigninChoiceCallback callback,
                            signin::SigninChoice choice) {
  std::move(callback).Run(choice);
  if (choice != signin::SIGNIN_CHOICE_CANCEL) {
    return;
  }
  // Depending on where the flow started:
  // - from main view: returns to the main view,
  // - from FRE: opens a signed out browser,
  // - from profile menu: closes the picker.
  ProfilePicker::CancelSignInFlow();
}

}  //  namespace

ProfilePickerPostSignInAdapter::ProfilePickerPostSignInAdapter(
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

ProfilePickerPostSignInAdapter::~ProfilePickerPostSignInAdapter() {
  if (contents()) {
    contents()->SetDelegate(nullptr);
  }
}

void ProfilePickerPostSignInAdapter::Init(
    StepSwitchFinishedCallback step_switch_callback) {
  DCHECK(!IsInitialized());
  CHECK(!step_switch_callback->is_null());
  CHECK(step_switch_callback_->is_null());
  step_switch_callback_ = std::move(step_switch_callback);

  contents()->SetDelegate(this);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  const AccountInfo& account_info =
      identity_manager->FindExtendedAccountInfo(account_info_);
  DCHECK(!account_info.IsEmpty())
      << "A profile with a valid account must be passed in.";
  email_ = account_info.email;

  on_post_signin_in_finished_callback_ =
      HistorySyncOptinHelper::FlowCompletedCallback(
          base::IgnoreArgs<HistorySyncOptinHelper::ScreenChoiceResult>(
              base::BindOnce(
                  &ProfilePickerPostSignInAdapter::FinishAndOpenBrowser,
                  weak_ptr_factory_.GetWeakPtr(), PostHostClearedCallback())));

  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    history_sync_optin_helper_ = HistorySyncOptinHelper::Create(
        identity_manager, profile_, account_info, /*delegate=*/this,
        HistorySyncOptinHelper::LaunchContext::kInProfilePicker,
        signin_access_point_);
    history_sync_optin_helper_->StartHistorySyncOptinFlow();
    return;
  }

  // TurnSyncOnHelper deletes itself once done.
  new TurnSyncOnHelper(
      profile_, signin_access_point_,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      account_info.account_id,
      TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
      std::make_unique<ProfilePickerTurnSyncOnDelegate>(
          weak_ptr_factory_.GetWeakPtr(), profile_),
      base::BindOnce(&ProfilePickerPostSignInAdapter::FinishAndOpenBrowser,
                     weak_ptr_factory_.GetWeakPtr(),
                     PostHostClearedCallback()));
}

void ProfilePickerPostSignInAdapter::ShowHistorySyncOptinScreen(
    Profile*,
    HistorySyncOptinHelper::FlowCompletedCallback
        history_optin_completed_callback) {
  CHECK(history_optin_completed_callback.value());
  CHECK(on_post_signin_in_finished_callback_.value());
  std::vector<HistorySyncOptinHelper::FlowCompletedCallback> callbacks;
  callbacks.push_back(std::move(on_post_signin_in_finished_callback_));
  callbacks.push_back(std::move(history_optin_completed_callback));
  on_post_signin_in_finished_callback_ =
      CombineCallbacks<HistorySyncOptinHelper::FlowCompletedCallback,
                       HistorySyncOptinHelper::ScreenChoiceResult>(
          std::move(callbacks));

  // Finishes the sign-in process by moving to the history sync optin screen.
  CHECK(IsInitialized());
  if (!step_switch_callback_->is_null()) {
    std::move(step_switch_callback_.value()).Run(true);
  }
  host_->ShowScreen(
      contents(), GetHistorySyncOptinURL(),
      /*navigation_finished_closure=*/
      base::BindOnce(
          &ProfilePickerPostSignInAdapter::SwitchToHistorySyncOptinFinished,
          // Unretained is enough as the callback is
          // called by the owner of this instance.
          base::Unretained(this)));
}

void ProfilePickerPostSignInAdapter::ShowAccountManagementScreen(
    signin::SigninChoiceCallback on_account_management_screen_closed) {
  SwitchToManagedUserProfileNotice(
      ManagedUserProfileNoticeUI::ScreenType::kProfilePicker,
      base::BindOnce(&OnManagementUserChoice,
                     std::move(on_account_management_screen_closed)));
}

void ProfilePickerPostSignInAdapter::FinishFlowWithoutHistorySyncOptin() {
  CHECK(!on_post_signin_in_finished_callback_.value().is_null());
  std::move(on_post_signin_in_finished_callback_.value())
      .Run(HistorySyncOptinHelper::ScreenChoiceResult::kScreenSkipped);
}

void ProfilePickerPostSignInAdapter::Cancel() {}

void ProfilePickerPostSignInAdapter::FinishAndOpenBrowser(
    PostHostClearedCallback callback) {
  bool is_continue_callback = !callback->is_null();

  if (url_to_open_.is_valid()) {
    auto open_url_callback = PostHostClearedCallback(
        base::BindOnce(&OpenNewTabInBrowser, url_to_open_));
    std::vector<PostHostClearedCallback> callbacks;
    callbacks.push_back(std::move(open_url_callback));
    callbacks.push_back(std::move(callback));
    callback = CombineCallbacks<PostHostClearedCallback, Browser*>(
        std::move(callbacks));
  }

  FinishAndOpenBrowserInternal(std::move(callback), is_continue_callback);
}

void ProfilePickerPostSignInAdapter::SwitchToSyncConfirmation() {
  DCHECK(IsInitialized());
  if (!step_switch_callback_->is_null()) {
    std::move(step_switch_callback_.value()).Run(true);
  }
  host_->ShowScreen(
      contents(), GetSyncConfirmationURL(/*loading=*/false),
      /*navigation_finished_closure=*/
      base::BindOnce(
          &ProfilePickerPostSignInAdapter::SwitchToSyncConfirmationFinished,
          // Unretained is enough as the callback is
          // called by the owner of this instance.
          base::Unretained(this)));
}

void ProfilePickerPostSignInAdapter::SwitchToManagedUserProfileNotice(
    ManagedUserProfileNoticeUI::ScreenType type,
    signin::SigninChoiceCallback process_user_choice_callback) {
  DCHECK(IsInitialized());
  if (!step_switch_callback_->is_null()) {
    std::move(step_switch_callback_.value()).Run(true);
  }
  host_->ShowScreen(contents(),
                    GURL(chrome::kChromeUIManagedUserProfileNoticeUrl),
                    /*navigation_finished_closure=*/
                    base::BindOnce(&ProfilePickerPostSignInAdapter::
                                       SwitchToManagedUserProfileNoticeFinished,
                                   // Unretained is enough as the callback is
                                   // called by the owner of this instance.
                                   base::Unretained(this), type,
                                   std::move(process_user_choice_callback)));
}

void ProfilePickerPostSignInAdapter::SwitchToProfileSwitch(
    const base::FilePath& profile_path) {
  DCHECK(IsInitialized());
  if (!step_switch_callback_->is_null()) {
    std::move(step_switch_callback_.value()).Run(true);
  }
  // The sign-in flow is finished, no profile window should be shown in the end.
  Cancel();

  GURL profile_switch_url(chrome::kChromeUIProfilePickerUrl);
  profile_switch_url = profile_switch_url.Resolve("profile-switch");
  // Appends the `profile_path` to be retrieved in the web page.
  profile_switch_url = net::AppendQueryParameter(
      profile_switch_url, "profileSwitchPath", base::ToString(profile_path));

  host_->ShowScreenInPickerContents(profile_switch_url, base::OnceClosure());
}

void ProfilePickerPostSignInAdapter::ResetHostAndShowErrorDialog(
    const ForceSigninUIError& error) {
  CHECK(IsInitialized());
  if (!step_switch_callback_->is_null()) {
    std::move(step_switch_callback_.value()).Run(false);
  }

  Cancel();
  host_->Reset(StepSwitchFinishedCallback(
      base::BindOnce(&ProfilePickerWebContentsHost::ShowForceSigninErrorDialog,
                     base::Unretained(host_), error)));
}

std::optional<SkColor> ProfilePickerPostSignInAdapter::GetProfileColor() const {
  // The new profile theme may be overridden by an existing policy theme. This
  // check ensures the correct theme is applied to the sync confirmation window.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service->UsingPolicyTheme()) {
    return theme_service->GetPolicyThemeColor();
  }
  return profile_color_;
}

GURL ProfilePickerPostSignInAdapter::GetSyncConfirmationURL(bool loading) {
  GURL url = GURL(chrome::kChromeUISyncConfirmationURL);
  return AppendSyncConfirmationQueryParams(
      loading ? url.Resolve(chrome::kChromeUISyncConfirmationLoadingPath) : url,
      SyncConfirmationStyle::kWindow, /*is_sync_promo=*/true);
}

std::unique_ptr<content::WebContents>
ProfilePickerPostSignInAdapter::ReleaseContents() {
  return std::move(contents_);
}

bool ProfilePickerPostSignInAdapter::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

bool ProfilePickerPostSignInAdapter::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return host_->GetWebContentsDelegate()->HandleKeyboardEvent(source, event);
}

void ProfilePickerPostSignInAdapter::SwitchToSyncConfirmationFinished() {
  DCHECK(IsInitialized());
  // Initialize the WebUI page once we know it's committed.
  SyncConfirmationUI* sync_confirmation_ui =
      static_cast<SyncConfirmationUI*>(contents()->GetWebUI()->GetController());

  sync_confirmation_ui->InitializeMessageHandlerWithBrowser(nullptr);
}

void ProfilePickerPostSignInAdapter::SwitchToHistorySyncOptinFinished() {
  CHECK(IsInitialized());
  // Initialize the WebUI page once we know it's committed.
  HistorySyncOptinUI* history_sync_optin_ui =
      static_cast<HistorySyncOptinUI*>(contents()->GetWebUI()->GetController());
  CHECK(!on_post_signin_in_finished_callback_->is_null());
  history_sync_optin_ui->Initialize(
      /*browser=*/nullptr,
      // Note: the value of `should_close_modal_dialog` does not matter, it has
      // no effect when `browser` is set to null.
      /*should_close_modal_dialog=*/std::nullopt,
      std::move(on_post_signin_in_finished_callback_));
}

void ProfilePickerPostSignInAdapter::SwitchToManagedUserProfileNoticeFinished(
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
          /*user_already_signed_in=*/false,
          /*profile_creation_required_by_policy=*/false,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          std::move(process_user_choice_callback),
          /*done_callback=*/base::OnceClosure()));
}

bool ProfilePickerPostSignInAdapter::IsInitialized() const {
  // `email_` is set in Init(), use it as the proxy here.
  return !email_.empty();
}
