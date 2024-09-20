// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGNED_IN_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGNED_IN_FLOW_CONTROLLER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/skia/include/core/SkColor.h"

class Profile;

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
class WebContents;
}  // namespace content

// Class triggering the signed-in section of the profile management flow, most
// notably featuring the sync confirmation. This class:
// - Expects a primary account to be set with `ConsentLevel::kSignin`.
// - Runs the `TurnSyncOnHelper` and provides it a delegate to interact with
//   `host`.
// - At the end of the flow we are in one of these cases:
//   - The host is closed and a browser is opened, via `FinishAndOpenBrowser()`;
//   - The host is not closed and the profile switch screen is shown, via
//     `SwitchToProfileSwitch()`.
class ProfilePickerSignedInFlowController
    : public content::WebContentsDelegate {
 public:
  ProfilePickerSignedInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents,
      signin_metrics::AccessPoint signin_access_point,
      std::optional<SkColor> profile_color);
  ~ProfilePickerSignedInFlowController() override;
  ProfilePickerSignedInFlowController(
      const ProfilePickerSignedInFlowController&) = delete;
  ProfilePickerSignedInFlowController& operator=(
      const ProfilePickerSignedInFlowController&) = delete;

  // Inits the flow, must be called before any other calls below.
  virtual void Init();

  // Cancels the flow explicitly.
  // By default does not do anything, in the flow it will be as if the dialog
  // was closed.
  virtual void Cancel();
  // Resets the host by redirecting to the main profile picker screen and
  // canceling the ongoing signed in flow. Shows an error dialog when the reset
  // is done.
  void ResetHostAndShowErrorDialog(const ForceSigninUIError& error);

  // Finishes the creation flow for `profile_`: marks it fully created,
  // transitions from `host_` to a new browser window and calls `callback` if
  // the browser window was successfully opened.
  // TODO(crbug.com/40242414): Tighten this contract by notifying the caller if
  // the browser open was not possible.
  void FinishAndOpenBrowser(PostHostClearedCallback callback);

  // Finishes the sign-in process by moving to the sync confirmation screen.
  virtual void SwitchToSyncConfirmation();

  // Finishes the sign-in process by moving to the managed user profile notice
  // screen.
  virtual void SwitchToManagedUserProfileNotice(
      ManagedUserProfileNoticeUI::ScreenType type,
      signin::SigninChoiceCallback proceed_callback);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The default implementation is NOTREACHED
  virtual void SwitchToLacrosIntro(
      signin::SigninChoiceCallback proceed_callback);
#endif

  // When the sign-in flow cannot be completed because another profile at
  // `profile_path` is already syncing with a chosen account, shows the profile
  // switch screen. It uses the system profile for showing the switch screen.
  void SwitchToProfileSwitch(const base::FilePath& profile_path);

  base::WeakPtr<ProfilePickerSignedInFlowController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Getter of the path of profile which is displayed on the profile switch
  // screen. Returns an empty path if no such screen has been displayed.
  base::FilePath switch_profile_path() const { return switch_profile_path_; }

  content::WebContents* contents() const { return contents_.get(); }

 protected:
  virtual void FinishAndOpenBrowserInternal(PostHostClearedCallback callback,
                                            bool is_continue_callback) = 0;

  // Returns the profile color, taking into account current policies.
  std::optional<SkColor> GetProfileColor() const;

  // Returns the URL for sync confirmation screen (or for the "is-loading"
  // version of it, if `loading` is true).
  GURL GetSyncConfirmationURL(bool loading);

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

  // Callbacks that finalize initialization of WebUI pages.
  void SwitchToSyncConfirmationFinished();
  void SwitchToManagedUserProfileNoticeFinished(
      ManagedUserProfileNoticeUI::ScreenType type,
      signin::SigninChoiceCallback process_user_choice_callback);

  // Returns whether the flow is initialized (i.e. whether `Init()` has been
  // called).
  bool IsInitialized() const;

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

  // Email of the signed-in account. It is set after the user finishes the
  // sign-in flow on GAIA and Chrome receives the account info.
  std::string email_;

  // Path to a profile that should be displayed on the profile switch screen.
  base::FilePath switch_profile_path_;

  GURL url_to_open_;

  base::WeakPtrFactory<ProfilePickerSignedInFlowController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGNED_IN_FLOW_CONTROLLER_H_
