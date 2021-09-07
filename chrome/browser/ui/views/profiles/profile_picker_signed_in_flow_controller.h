// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGNED_IN_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGNED_IN_FLOW_CONTROLLER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
class ProfilePickerSignedInFlowController
    : public content::WebContentsDelegate,
      public signin::IdentityManager::Observer {
 public:
  using BrowserOpenedCallback = base::OnceCallback<void(Browser*)>;

  ProfilePickerSignedInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      std::unique_ptr<content::WebContents> contents,
      absl::optional<SkColor> profile_color,
      base::TimeDelta extended_account_info_timeout);
  ~ProfilePickerSignedInFlowController() override;
  ProfilePickerSignedInFlowController(
      const ProfilePickerSignedInFlowController&) = delete;
  ProfilePickerSignedInFlowController& operator=(
      const ProfilePickerSignedInFlowController&) = delete;

  // Inits the flow, must be called before any other calls below.
  void Init(bool is_saml);

  // Getter of the path of profile which is displayed on the profile switch
  // screen. Returns an empty path if no such screen has been displayed.
  base::FilePath switch_profile_path() const { return switch_profile_path_; }

  // Cancels the flow explicitly. This does not log any metrics, the caller
  // must take care of logging the outcome of the flow on its own.
  void Cancel();

  // Finishes the creation flow by marking `profile_being_created_` as fully
  // created, opening a browser window for this profile and calling
  // `callback`.
  void FinishAndOpenBrowser(BrowserOpenedCallback callback);

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

  // IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;

  // Helper functions to deal with the lack of extended account info.
  void OnExtendedAccountInfoTimeout(const CoreAccountInfo& account);
  void OnProfileNameAvailable();

  // Callbacks that finalize initialization of WebUI pages.
  void SwitchToSyncConfirmationFinished();
  void SwitchToEnterpriseProfileWelcomeFinished(
      EnterpriseProfileWelcomeUI::ScreenType type,
      base::OnceCallback<void(bool)> proceed_callback);

  // Returns whether the flow is initialized (i.e. whether `Init()` has been
  // called).
  bool IsInitialized() const;

  // Returns the profile color, taking into account current policies.
  absl::optional<SkColor> GetProfileColor() const;

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

  content::WebContents* contents() const { return contents_.get(); }

  // The host object, must outlive this object.
  ProfilePickerWebContentsHost* host_;

  Profile* profile_ = nullptr;

  // Prevent |profile_| from being destroyed first.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  // The web contents backed by `profile`. This is used for displaying the
  // sign-in flow.
  std::unique_ptr<content::WebContents> contents_;

  // Set for the profile at the very end to avoid coloring the simple toolbar
  // for GAIA sign-in (that uses the ThemeProvider of the current profile).
  // absl::nullopt if the profile should use the default theme.
  absl::optional<SkColor> profile_color_;

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

  base::WeakPtrFactory<ProfilePickerSignedInFlowController> weak_ptr_factory_{
      this};
};

// TODO(crbug.com/1227029): Move into a separate file.
// Class responsible for the GAIA sign-in within profile creation flow.
class ProfilePickerDiceSignInProvider
    : public content::WebContentsDelegate,
      public ChromeWebModalDialogManagerDelegate,
      public signin::IdentityManager::Observer {
 public:
  // The callback returns the newly created profile and a valid WebContents
  // instance within this profile. If `is_saml` is true, sign-in is not
  // completed there yet. Otherwise, the newly created profile is properly
  // signed-in, i.e. its IdentityManager has a (unconsented) primary account.
  // If the flow gets canceled by closing the window, the callback never gets
  // called.
  // TODO(crbug.com/1240650): Properly support saml sign in so that the special
  // casing is not needed here.
  using SignedInCallback =
      base::OnceCallback<void(Profile* profile,
                              std::unique_ptr<content::WebContents>,
                              bool is_saml)>;

  explicit ProfilePickerDiceSignInProvider(ProfilePickerWebContentsHost* host);
  ~ProfilePickerDiceSignInProvider() override;
  ProfilePickerDiceSignInProvider(const ProfilePickerDiceSignInProvider&) =
      delete;
  ProfilePickerDiceSignInProvider& operator=(
      const ProfilePickerDiceSignInProvider&) = delete;

  // Initiates switching the flow to sign-in (which is normally asynchronous).
  // If a sign-in was in progress before in the lifetime of this class, it only
  // (synchronously) switches the view to show the ongoing sign-in again. When
  // the sign-in screen is displayed, `switch_finished_callback` gets called.
  // When the sign-in finishes (if it ever happens), `signin_finished_callback`
  // gets called.
  void SwitchToSignIn(base::OnceCallback<void(bool)> switch_finished_callback,
                      SignedInCallback signin_finished_callback);

  // Reloads the sign-in page if applicable.
  void ReloadSignInPage();

  // Returns theme provider based on the sign-in profile or nullptr if the flow
  // is not yet initialized.
  const ui::ThemeProvider* GetThemeProvider() const;

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
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // Initializes the flow with the newly created profile.
  void OnProfileCreated(
      base::OnceCallback<void(bool)>& switch_finished_callback,
      Profile* new_profile,
      Profile::CreateStatus status);

  // Finishes the sign-in (if there is a primary account with refresh tokens).
  void FinishFlowIfSignedIn();

  // Finishes the sign-in (if `is_saml` is true, it's due to SAML signin getting
  // detected).
  void FinishFlow(bool is_saml);

  // Returns whether the flow is initialized (i.e. whether `profile_` has been
  // created).
  bool IsInitialized() const;

  void OnSignInContentsFreedUp();

  content::WebContents* contents() const { return contents_.get(); }

  // The host object, must outlive this object.
  ProfilePickerWebContentsHost* host_;
  // Sign-in callback, valid until it's called.
  SignedInCallback callback_;

  Profile* profile_ = nullptr;

  // Prevent |profile_| from being destroyed first.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  // The web contents backed by `profile`. This is used for displaying the
  // sign-in flow.
  std::unique_ptr<content::WebContents> contents_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<ProfilePickerDiceSignInProvider> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_SIGNED_IN_FLOW_CONTROLLER_H_
