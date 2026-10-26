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
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
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
void OpenNewTabInBrowser(const GURL& url, BrowserWindowInterface* browser) {
  if (browser) {
    browser->OpenGURL(url, WindowOpenDisposition::kSingletonTab);
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
  CHECK(profile_, base::NotFatalUntil::M162);
  CHECK(contents_, base::NotFatalUntil::M162);
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
  CHECK(!IsInitialized(), base::NotFatalUntil::M162);
  CHECK(!step_switch_callback->is_null());
  CHECK(step_switch_callback_->is_null());
  step_switch_callback_ = std::move(step_switch_callback);

  contents()->SetDelegate(this);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  const AccountInfo& account_info =
      identity_manager->FindExtendedAccountInfo(account_info_);
  DCHECK(!account_info.IsEmpty())
      << "A profile with a valid account must be passed in.";
  email_ = account_info.GetEmail();

  on_post_signin_in_finished_callback_ =
      HistorySyncOptinHelper::FlowCompletedCallback(
          base::IgnoreArgs<HistorySyncOptinHelper::ScreenChoiceResult>(
              base::BindOnce(
                  &ProfilePickerPostSignInAdapter::FinishAndOpenBrowser,
                  weak_ptr_factory_.GetWeakPtr())));

  history_sync_optin_helper_ = HistorySyncOptinHelper::Create(
      identity_manager, profile_, account_info, /*delegate=*/this,
      HistorySyncOptinHelper::LaunchContext::kInProfilePicker,
      signin_access_point_);
  history_sync_optin_helper_->StartHistorySyncOptinFlow();
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
  ManagedUserProfileNoticeUI::ScreenType screen_type =
      signin_access_point_ == signin_metrics::AccessPoint::kForYouFre
          ? ManagedUserProfileNoticeUI::ScreenType::kFirstRun
          : ManagedUserProfileNoticeUI::ScreenType::kProfilePicker;
  SwitchToManagedUserProfileNotice(
      screen_type,
      base::BindOnce(&OnManagementUserChoice,
                     std::move(on_account_management_screen_closed)));
}

void ProfilePickerPostSignInAdapter::FinishFlowWithoutHistorySyncOptin() {
  CHECK(!on_post_signin_in_finished_callback_.value().is_null());
  std::move(on_post_signin_in_finished_callback_.value())
      .Run(HistorySyncOptinHelper::ScreenChoiceResult::kScreenSkipped);
}

void ProfilePickerPostSignInAdapter::Cancel() {}

void ProfilePickerPostSignInAdapter::FinishAndOpenBrowser() {
  PostHostClearedCallback callback;
  if (url_to_open_.is_valid()) {
    callback = PostHostClearedCallback(
        base::BindOnce(&OpenNewTabInBrowser, url_to_open_));
  }

  FinishAndOpenBrowserInternal(std::move(callback));
}

void ProfilePickerPostSignInAdapter::SwitchToManagedUserProfileNotice(
    ManagedUserProfileNoticeUI::ScreenType type,
    signin::SigninChoiceCallback process_user_choice_callback) {
  CHECK(IsInitialized(), base::NotFatalUntil::M162);
  if (!step_switch_callback_->is_null()) {
    std::move(step_switch_callback_.value()).Run(true);
  }

  ManagedUserProfileNoticeParams::CreateForWebContents(
      contents(), /*browser=*/nullptr, type,
      std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
          GetAccountInfo(),
          /*is_oidc_account=*/type ==
              ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC,
          /*user_already_signed_in=*/false,
          /*profile_creation_required_by_policy=*/false,
          /*show_link_data_option=*/false,
          /*process_user_choice_callback=*/
          std::move(process_user_choice_callback),
          /*done_callback=*/base::OnceClosure()));

  host_->ShowScreen(
      contents(), ManagedUserProfileNoticeUI::GetUrl(type, *profile_),
      /*navigation_finished_closure=*/
      base::BindOnce(
          [](content::WebContents* web_contents) {
            CHECK(
                !ManagedUserProfileNoticeParams::FromWebContents(web_contents))
                << "ManagedUserProfileNoticeParams were not consumed.";
          },
          contents()));
}

AccountInfo ProfilePickerPostSignInAdapter::GetAccountInfo() const {
  AccountInfo extended_info =
      IdentityManagerFactory::GetForProfile(profile_)->FindExtendedAccountInfo(
          account_info_);
  if (!extended_info.IsEmpty()) {
    return extended_info;
  }
  return AccountInfo::Builder(account_info_).Build();
}

void ProfilePickerPostSignInAdapter::ShowSignInCelebration(
    base::OnceClosure celebration_finished) {
  host_->ShowScreen(
      contents(),
      GURL(chrome::kChromeUIIntroURL)
          .Resolve(chrome::kChromeUIIntroSignInCelebrationSubPage),
      base::BindOnce(
          &ProfilePickerPostSignInAdapter::SwitchToSignInCelebrationFinished,
          base::Unretained(this), std::move(celebration_finished)));
}

void ProfilePickerPostSignInAdapter::SwitchToSignInCelebrationFinished(
    base::OnceClosure celebration_finished) {
  IntroUI* intro_ui = contents()->GetWebUI()->GetController()->GetAs<IntroUI>();
  CHECK(intro_ui);
  intro_ui->SetSignInCelebrationFinishedCallback(
      std::move(celebration_finished));
}

std::optional<SkColor> ProfilePickerPostSignInAdapter::GetProfileColor() const {
  // The new profile theme may be overridden by an existing policy theme. This
  // check ensures the correct theme is applied to the history sync optin
  // window.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service->UsingPolicyTheme()) {
    return theme_service->GetPolicyThemeColor();
  }
  return profile_color_;
}

GURL ProfilePickerPostSignInAdapter::GetLoadingScreenURL() const {
  return GURL(chrome::kChromeUIHistorySyncOptinURL)
      .Resolve(chrome::kChromeUIHistorySyncOptinLoadingPath);
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

bool ProfilePickerPostSignInAdapter::IsInitialized() const {
  // `email_` is set in Init(), use it as the proxy here.
  return !email_.empty();
}
