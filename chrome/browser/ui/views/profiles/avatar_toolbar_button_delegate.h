// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"

class Browser;
class Profile;

// Handles the business logic for AvatarToolbarButton. This includes
// managing the highlight animation and the identity animation.
class AvatarToolbarButtonDelegate : public BrowserListObserver,
                                    public ProfileAttributesStorage::Observer,
                                    public signin::IdentityManager::Observer,
                                    public syncer::SyncServiceObserver {
 public:
  AvatarToolbarButtonDelegate(AvatarToolbarButton* button, Browser* browser);

  AvatarToolbarButtonDelegate(const AvatarToolbarButtonDelegate&) = delete;
  AvatarToolbarButtonDelegate& operator=(const AvatarToolbarButtonDelegate&) =
      delete;

  ~AvatarToolbarButtonDelegate() override;

  // Methods called by the AvatarToolbarButton to get profile information.
  std::u16string GetProfileName() const;
  std::u16string GetShortProfileName() const;
  gfx::Image GetGaiaAccountImage() const;
  // Must only be called in states which have an avatar image (i.e. not
  // kGuestSession and not kIncognitoProfile).
  gfx::Image GetProfileAvatarImage(gfx::Image gaia_account_image,
                                   int preferred_size) const;

  // Returns the count of incognito or guest windows attached to the profile.
  int GetWindowCount() const;

  AvatarToolbarButton::State GetState() const;

  absl::optional<AvatarSyncErrorType> GetAvatarSyncErrorType() const;

  bool IsSyncFeatureEnabled() const;

  void ShowHighlightAnimation();
  bool IsHighlightAnimationVisible() const;

#if !BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_CHROMEOS_ASH)
  void ShowSignInText();
  void HideSignInText();
#endif

  // Should be called when the icon is updated. This may trigger the identity
  // pill animation if the delegate is waiting for the image.
  void MaybeShowIdentityAnimation(const gfx::Image& gaia_account_image);

  // Enables or disables the IPH highlight.
  void SetHasInProductHelpPromo(bool has_promo);

  // Called by the AvatarToolbarButton to notify the delegate about events.
  void NotifyClick();
  void OnMouseExited();
  void OnBlur();

 private:
  enum class ButtonTextState {
    kNotShowing,
    kWaitingForImage,
    kShowingName,
    kShowingSigninText
  };

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const std::u16string& old_profile_name) override;

  // IdentityManager::Observer:
  // Needed if the first sync promo account should be displayed.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnRefreshTokensLoaded() override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;
  void OnIdentityManagerShutdown(signin::IdentityManager*) override;

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService*) override;
  void OnSyncShutdown(syncer::SyncService*) override;

  // Initiates showing the identity.
  void OnUserIdentityChanged();

  void OnIdentityAnimationTimeout();
  // Called after the user interacted with the button or after some timeout.
  void MaybeHideIdentityAnimation();
  void HideHighlightAnimation();

  // Shows the identity pill animation. If the animation is already showing,
  // this extends the duration of the current animation.
  void ShowIdentityAnimation();

  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  const raw_ptr<AvatarToolbarButton> avatar_toolbar_button_;
  const raw_ptr<Browser> browser_;
  const raw_ptr<Profile> profile_;
  ButtonTextState button_text_state_ = ButtonTextState::kNotShowing;

  // Count of identity pill animation timeouts that are currently scheduled.
  // Multiple timeouts are scheduled when multiple animation triggers happen in
  // a quick sequence (before the first timeout passes). The identity pill tries
  // to close when this reaches 0.
  int identity_animation_timeout_count_ = 0;

  bool refresh_tokens_loaded_ = false;
  bool has_in_product_help_promo_ = false;

  // Whether the avatar highlight animation is visible. The animation is shown
  // when an Autofill datatype is saved. When this is true the avatar button
  // sync paused/error state will be disabled.
  bool highlight_animation_visible_ = false;

  // Caches the value of the last error so the class can detect when it changes
  // and notify |avatar_toolbar_button_|.
  absl::optional<AvatarSyncErrorType> last_avatar_error_;

  base::WeakPtrFactory<AvatarToolbarButtonDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_
