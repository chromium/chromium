// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_utils.h"

#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "google_apis/gaia/core_account_id.h"
#include "ui/base/webui/web_ui_util.h"

namespace signin {

namespace {
#if !(BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS))
// Default timeout used to wait for account capabilities fetch.
const int kMinorModeRestrictionsFetchDeadlineMs = 1000;
#endif

}  // namespace

EnterpriseProfileCreationDialogParams::EnterpriseProfileCreationDialogParams(
    AccountInfo account_info,
    bool is_oidc_account,
    bool profile_creation_required_by_policy,
    bool show_link_data_option,
    SigninChoiceCallbackVariant process_user_choice_callback,
    base::OnceClosure done_callback,
    base::OnceClosure retry_callback)
    : account_info(account_info),
      is_oidc_account(is_oidc_account),
      profile_creation_required_by_policy(profile_creation_required_by_policy),
      show_link_data_option(show_link_data_option),
      process_user_choice_callback(std::move(process_user_choice_callback)),
      done_callback(std::move(done_callback)),
      retry_callback(std::move(retry_callback)) {}

EnterpriseProfileCreationDialogParams::
    ~EnterpriseProfileCreationDialogParams() = default;

content::RenderFrameHost* GetAuthFrame(content::WebContents* web_contents,
                                       const std::string& parent_frame_name) {
  content::RenderFrameHost* frame = nullptr;
  web_contents->ForEachRenderFrameHostWithAction(
      [&frame, &parent_frame_name](content::RenderFrameHost* rfh) {
        auto* web_view = extensions::WebViewGuest::FromRenderFrameHost(rfh);
        if (web_view && web_view->name() == parent_frame_name) {
          DCHECK_EQ(web_view->GetGuestMainFrame(), rfh);
          frame = rfh;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return frame;
}

extensions::WebViewGuest* GetAuthWebViewGuest(
    content::WebContents* web_contents,
    const std::string& parent_frame_name) {
  return extensions::WebViewGuest::FromRenderFrameHost(
      GetAuthFrame(web_contents, parent_frame_name));
}

Browser* GetDesktopBrowser(content::WebUI* web_ui) {
  Browser* browser = chrome::FindBrowserWithTab(web_ui->GetWebContents());
  if (!browser)
    browser = chrome::FindLastActiveWithProfile(Profile::FromWebUI(web_ui));
  return browser;
}

base::TimeDelta GetMinorModeRestrictionsDeadline() {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Not implemented for those platforms.
  NOTREACHED();
#else
  return base::Milliseconds(kMinorModeRestrictionsFetchDeadlineMs);
#endif
}

void SetInitializedModalHeight(Browser* browser,
                               content::WebUI* web_ui,
                               const base::Value::List& args) {
  if (!browser)
    return;

  double height = args[0].GetDouble();
  browser->signin_view_controller()->SetModalSigninHeight(
      static_cast<int>(height));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void ClearProfileWithManagedAccounts(Profile* profile) {
  policy::UserPolicySigninServiceFactory::GetForProfile(profile)
      ->ShutdownCloudPolicyManager();
  enterprise_util::SetUserAcceptedAccountManagement(profile, false);

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (!primary_account_id.empty()) {
    identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        signin_metrics::ProfileSignout::kAbortSignin);
  }
}
#endif

std::string GetAccountPictureUrl(const AccountInfo& account_info) {
  return account_info.account_image.IsEmpty()
             ? profiles::GetPlaceholderAvatarIconUrl()
             : webui::GetBitmapDataUrl(account_info.account_image.AsBitmap());
}

}  // namespace signin
