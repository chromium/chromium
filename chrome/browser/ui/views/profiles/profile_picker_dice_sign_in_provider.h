// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_SIGN_IN_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_SIGN_IN_PROVIDER_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

struct CoreAccountInfo;
class ProfilePickerWebContentsHost;

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
class WebContents;
}  // namespace content

// Class responsible for the GAIA sign-in within profile management flows.
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
                              bool is_saml,
                              std::unique_ptr<content::WebContents>)>;

  // Creates a new provider that will render the Gaia sign-in flow in `host` for
  // a profile at `profile_path`.
  // If no `profile_path` is provided, a new profile (and associated directory)
  // will be created.
  explicit ProfilePickerDiceSignInProvider(
      ProfilePickerWebContentsHost* host,
      signin_metrics::AccessPoint signin_access_point,
      absl::optional<base::FilePath> profile_path = absl::nullopt);
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

  // Navigates back in the sign-in flow if applicable.
  void NavigateBack();

  // Returns whether the flow is initialized (i.e. whether `profile_` has been
  // loaded).
  bool IsInitialized() const;

  content::WebContents* contents() const { return contents_.get(); }

 private:
  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
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

  // Initializes the flow with the newly created or loaded profile.
  void OnProfileInitialized(
      base::OnceCallback<void(bool)> switch_finished_callback,
      Profile* new_profile);

  // Finishes the sign-in (if there is a primary account with refresh tokens).
  void FinishFlowIfSignedIn();

  // Finishes the sign-in (if `is_saml` is true, it's due to SAML signin getting
  // detected).
  void FinishFlow(bool is_saml);

  void OnSignInContentsFreedUp();

  GURL BuildSigninURL() const;

  // The host must outlive this object.
  const raw_ptr<ProfilePickerWebContentsHost> host_;

  const signin_metrics::AccessPoint signin_access_point_;

  // The path to the profile in which to perform the sign-in. If absent, a new
  // profile will be created.
  const absl::optional<base::FilePath> profile_path_;
  // Sign-in callback, valid until it's called.
  SignedInCallback callback_;

  raw_ptr<Profile> profile_ = nullptr;

  // Prevent |profile_| from being destroyed first.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  // The web contents backed by `profile_`. This is used for displaying the
  // sign-in flow.
  std::unique_ptr<content::WebContents> contents_;

  // Because of ProfileOAuth2TokenService intricacies, the sign in should not
  // finish before both the notification gets called.
  // TODO(crbug.com/1249488): Remove this if the bug gets resolved.
  bool refresh_token_updated_ = false;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<ProfilePickerDiceSignInProvider> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_SIGN_IN_PROVIDER_H_
