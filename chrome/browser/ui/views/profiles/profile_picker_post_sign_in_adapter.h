// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_POST_SIGN_IN_ADAPTER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_POST_SIGN_IN_ADAPTER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

class Profile;
namespace content {
struct ContextMenuParams;
class RenderFrameHost;
class WebContents;
}  // namespace content

// Class triggering the signed-in section of the profile management flow, most
// notably featuring the history sync optin. This class:
// - Expects a primary account to be set with `ConsentLevel::kSignin`.
// - Creates the `HistorySyncOptinHelper` and acts as its delegate to interact
//   with `host`.
// - At the end of the flow the host is closed and a browser is opened, via
//   `FinishAndOpenBrowser()`.
class ProfilePickerPostSignInAdapter : public content::WebContentsDelegate,
                                       public HistorySyncOptinHelper::Delegate {
 public:
  ProfilePickerPostSignInAdapter(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents,
      signin_metrics::AccessPoint signin_access_point,
      std::optional<SkColor> profile_color);
  ~ProfilePickerPostSignInAdapter() override;
  ProfilePickerPostSignInAdapter(const ProfilePickerPostSignInAdapter&) =
      delete;
  ProfilePickerPostSignInAdapter& operator=(
      const ProfilePickerPostSignInAdapter&) = delete;

  // Inits the flow, must be called before any other calls below.
  virtual void Init(StepSwitchFinishedCallback step_switch_callback);

  // Cancels the flow explicitly.
  // By default does not do anything, in the flow it will be as if the dialog
  // was closed.
  virtual void Cancel();

  // HistorySyncOptinHelper::Delegate implementation:
  void ShowHistorySyncOptinScreen(
      Profile*,
      HistorySyncOptinHelper::FlowCompletedCallback
          history_optin_completed_callback) override;
  void ShowAccountManagementScreen(
      signin::SigninChoiceCallback on_account_management_screen_closed)
      override;
  void FinishFlowWithoutHistorySyncOptin() override;
  void ShowSignInCelebration(
      base::OnceClosure celebration_finished) override;

  base::WeakPtr<ProfilePickerPostSignInAdapter> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  content::WebContents* contents() const { return contents_.get(); }

  signin_metrics::AccessPoint signin_access_point() const {
    return signin_access_point_;
  }

 protected:
  virtual void FinishAndOpenBrowserInternal(
      PostHostClearedCallback callback) = 0;

  // Returns the profile color, taking into account current policies.
  std::optional<SkColor> GetProfileColor() const;

  // Returns the URL for the post-sign-in loading spinner screen.
  GURL GetLoadingScreenURL() const;

  ProfilePickerWebContentsHost* host() const { return host_; }
  Profile* profile() const { return profile_; }
  std::unique_ptr<content::WebContents> ReleaseContents();
  const CoreAccountInfo& account_info() const { return account_info_; }

 private:
  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  // Finishes the creation flow for `profile_`: marks it fully created,
  // transitions from `host_` to a new browser window and calls
  // `FinishAndOpenBrowserInternal()`.
  // TODO(crbug.com/40242414): Tighten this contract by notifying the caller if
  // the browser open was not possible.
  void FinishAndOpenBrowser();

  // Finishes the sign-in process by moving to the managed user profile notice
  // screen.
  void SwitchToManagedUserProfileNotice(
      ManagedUserProfileNoticeUI::ScreenType type,
      signin::SigninChoiceCallback proceed_callback);

  // Callbacks that finalize initialization of WebUI pages.
  void SwitchToHistorySyncOptinFinished();
  void SwitchToSignInCelebrationFinished(base::OnceClosure celebration_finished);

  // Returns whether the flow is initialized (i.e. whether `Init()` has been
  // called).
  bool IsInitialized() const;

  // Returns the account info for the signed-in account.
  AccountInfo GetAccountInfo() const;

  // The host object, must outlive this object.
  raw_ptr<ProfilePickerWebContentsHost> host_;

  raw_ptr<Profile> profile_ = nullptr;

  // Account ID for the profile. Note that it may not be set as primary account
  // yet.
  const CoreAccountInfo account_info_;

  // Prevent |profile_| from being destroyed first.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  // The web contents backed by `profile`. This is used for displaying the
  // sign-in flow.
  std::unique_ptr<content::WebContents> contents_;

  const signin_metrics::AccessPoint signin_access_point_;

  // Set for the profile at the very end to avoid coloring the simple toolbar
  // for GAIA sign-in (that uses the ThemeProvider of the current profile).
  // std::nullopt if the profile should use the default theme.
  std::optional<SkColor> profile_color_;

  // This callback will only return once the content of the step decided if it
  // should be shown or not. E.g.: History sync opt-in screen or enterprise
  // management.
  StepSwitchFinishedCallback step_switch_callback_;

  // Steps to be executed after the user has made a choice in the history sync
  // optin screen. Might be executed when the screen is also skipped.
  HistorySyncOptinHelper::FlowCompletedCallback
      on_post_signin_in_finished_callback_;

  std::unique_ptr<HistorySyncOptinHelper> history_sync_optin_helper_;

  // Email of the signed-in account. It is set after the user finishes the
  // sign-in flow on GAIA and Chrome receives the account info.
  std::string email_;

  GURL url_to_open_;

  base::WeakPtrFactory<ProfilePickerPostSignInAdapter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_POST_SIGN_IN_ADAPTER_H_
