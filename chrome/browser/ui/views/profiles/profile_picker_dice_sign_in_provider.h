// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_SIGN_IN_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_SIGN_IN_PROVIDER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents_delegate.h"

struct CoreAccountInfo;
class DiceTabHelper;
class ProfilePickerWebContentsHost;

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
class WebContents;
}  // namespace content

// Class responsible for the GAIA sign-in within profile management flows.
class ProfilePickerDiceSignInProvider
    : public content::WebContentsDelegate,
      public ChromeWebModalDialogManagerDelegate {
 public:
  // The callback returns the newly created profile and a valid WebContents
  // instance within this profile. If the account info is empty, sign-in is not
  // completed there yet. Otherwise, the newly created profile has the account
  // in its `IdentityManager`, but the account may not be set as primary yet.
  // If the flow gets canceled by closing the window, the callback never gets
  // called.
  // TODO(crbug.com/40785551): Properly support saml sign in so that the special
  // casing is not needed here.
  using SignedInCallback =
      base::OnceCallback<void(Profile*,
                              const CoreAccountInfo&,
                              std::unique_ptr<content::WebContents>)>;

  // Creates a new provider that will render the Gaia sign-in flow in `host` for
  // a profile at `profile_path`.
  // If no `profile_path` is provided, a new profile (and associated directory)
  // will be created.
  explicit ProfilePickerDiceSignInProvider(
      ProfilePickerWebContentsHost* host,
      signin_metrics::AccessPoint signin_access_point,
      base::FilePath profile_path = base::FilePath());
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
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // Initializes the flow with the newly created or loaded profile.
  void OnProfileInitialized(
      base::OnceCallback<void(bool)> switch_finished_callback,
      Profile* new_profile);

  // `account_info` is empty if the signin could not complete and must continue
  // in a browser (e.g. for SAML).
  void FinishFlow(const CoreAccountInfo& account_info);

  // Callback for the `DiceTabHelper`. Calls `FinishFlow()`.
  void FinishFlowInPicker(Profile* profile,
                          signin_metrics::AccessPoint access_point,
                          signin_metrics::PromoAction promo_action,
                          content::WebContents* contents,
                          const CoreAccountInfo& account_info);

  void OnSignInContentsFreedUp();

  GURL BuildSigninURL() const;

  // The sync confirmation screen is triggered by the `DiceTabHelper`. This can
  // happen in two cases: in the picker for normal accounts, and  in the
  // browser for SAML account.
  enum class DiceTabHelperMode {
    kInPicker,   // The sync confirmation screen opens in the picker.
    kInBrowser,  // The sync confirmation screen opens in the browser.
  };

  // Initializes the `DiceTabHelper`. It is initialized once at the beginning,
  // with the `kInPicker` mode, and in case of SAML it is initialized again
  // with the `kInBrowser` mode as the web contents is extracted to a tab.
  void InitializeDiceTabHelper(DiceTabHelper& helper, DiceTabHelperMode mode);

  // The host must outlive this object.
  const raw_ptr<ProfilePickerWebContentsHost> host_;

  const signin_metrics::AccessPoint signin_access_point_;

  // The path to the profile in which to perform the sign-in. If empty, a new
  // profile will be created.
  const base::FilePath profile_path_;
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
  // TODO(crbug.com/40791271): Remove this if the bug gets resolved.
  bool refresh_token_updated_ = false;

  base::WeakPtrFactory<ProfilePickerDiceSignInProvider> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_DICE_SIGN_IN_PROVIDER_H_
