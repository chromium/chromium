// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGN_IN_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGN_IN_FLOW_CONTROLLER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/skia/include/core/SkColor.h"

struct AccountInfo;
struct CoreAccountInfo;
class Browser;
class Profile;

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace ui {
class ThemeProvider;
}  // namespace ui

// Class responsible for the sign-in profile creation flow (within
// ProfilePickerView).
class ProfilePickerSignInFlowController
    : public content::WebContentsDelegate,
      public ChromeWebModalDialogManagerDelegate,
      public signin::IdentityManager::Observer {
 public:
  using BrowserOpenedCallback = base::OnceCallback<void(Browser*)>;

  ProfilePickerSignInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      SkColor profile_color,
      base::TimeDelta extended_account_info_timeout);
  ~ProfilePickerSignInFlowController() override;
  ProfilePickerSignInFlowController(const ProfilePickerSignInFlowController&) =
      delete;
  ProfilePickerSignInFlowController& operator=(
      const ProfilePickerSignInFlowController&) = delete;

  // Must be called after constructor.
  void Init();

  // Cancels the flow explicitly. This does not log any metrics, the caller
  // must take care of logging the outcome of the flow on its own.
  void Cancel();

  content::WebContents* contents() const { return contents_.get(); }

  // Updates the profile color provided in the constructor.
  void SetProfileColor(SkColor color);
  // Returns the profile color, taking into account current policies.
  SkColor GetProfileColor() const;

  bool IsSigningIn() const;

  // Returns theme provider based on `profile_`.
  const ui::ThemeProvider* GetThemeProvider() const;

  // Returns the domain of the email of the signed-in user or an empty string
  // if the user is not signed-in.
  std::string GetUserDomain() const;

  Profile* profile() const { return profile_; }

  // Getter of the path of profile which is displayed on the profile switch
  // screen. Returns an empty path if no such screen has been displayed.
  base::FilePath switch_profile_path() const { return switch_profile_path_; }

  // Finishes the creation flow by marking `profile_being_created_` as fully
  // created, opening a browser window for this profile and calling
  // `callback`.
  void FinishAndOpenBrowser(BrowserOpenedCallback callback,
                            bool enterprise_sync_consent_needed);

  // Finishes the sign-in process by moving to the sync confirmation screen.
  void SwitchToSyncConfirmation();

  // Finishes the sign-in process by moving to the enterprise profile welcome
  // screen.
  void SwitchToEnterpriseProfileWelcome(
      EnterpriseProfileWelcomeUI::ScreenType type,
      base::OnceCallback<void(bool)> proceed_callback);

  // When the sign-in flow cannot be completed because another profile at
  // `profile_path` is already syncing with a chosen account, shows the profile
  // switch screen. It uses the system profile for showing the switch screen.
  void SwitchToProfileSwitch(const base::FilePath& profile_path);

 private:
  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;

  // Helper functions to deal with the lack of extended account info.
  void OnExtendedAccountInfoTimeout(const CoreAccountInfo& account);
  void OnProfileNameAvailable();

  // Callbacks that finalize initialization of WebUI pages.
  void SwitchToSyncConfirmationFinished();
  void SwitchToEnterpriseProfileWelcomeFinished(
      EnterpriseProfileWelcomeUI::ScreenType type,
      base::OnceCallback<void(bool)> proceed_callback);

  void FinishAndOpenBrowserImpl(BrowserOpenedCallback callback,
                                bool enterprise_sync_consent_needed);

  // Finishes the flow by finalizing the profile and continuing the SAML
  // sign-in in a browser window.
  void FinishAndOpenBrowserForSAML();
  void OnSignInContentsFreedUp();

  // Internal callback to finish the last steps of the signed-in creation
  // flow.
  void OnBrowserOpened(BrowserOpenedCallback finish_flow_callback,
                       Profile* profile,
                       Profile::CreateStatus profile_create_status);

  // The host object, must outlive this object.
  ProfilePickerWebContentsHost* host_;

  // The web contents backed by `profile`. This is used for displaying the
  // sign-in flow.
  std::unique_ptr<content::WebContents> contents_;

  Profile* profile_ = nullptr;

  // Set for the profile at the very end to avoid coloring the simple toolbar
  // for GAIA sign-in (that uses the ThemeProvider of the current profile).
  SkColor profile_color_;

  // For finishing the profile creation flow, the extended account info is
  // needed (for properly naming the new profile). After a timeout, a fallback
  // name is used, instead, to unblock the flow.
  base::TimeDelta extended_account_info_timeout_;

  // Controls whether the flow still needs to finalize (which includes showing
  // `profile` browser window at the end of the sign-in flow).
  bool is_finished_ = false;

  // Email of the signed-in account. It is set after the user finishes the
  // sign-in flow on GAIA and Chrome receives the account info.
  std::string email_;

  // Path to a profile that should be displayed on the profile switch screen.
  base::FilePath switch_profile_path_;

  std::u16string name_for_signed_in_profile_;
  base::OnceClosure on_profile_name_available_;

  base::CancelableOnceClosure extended_account_info_timeout_closure_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<ProfilePickerSignInFlowController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGN_IN_FLOW_CONTROLLER_H_
