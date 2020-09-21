// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/avatar_button_error_controller.h"
#include "chrome/browser/ui/avatar_button_error_controller_delegate.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/gfx/image/image.h"

class Profile;

// Handles the business logic for AvatarToolbarButton. This includes managing
// the highlight animation and the identity animation.
class AvatarToolbarButtonDelegate : public BrowserListObserver,
                                    public ProfileAttributesStorage::Observer,
                                    public AvatarButtonErrorControllerDelegate,
                                    public signin::IdentityManager::Observer {
 public:
  AvatarToolbarButtonDelegate();
  ~AvatarToolbarButtonDelegate() override;

  // Must be called before the object can be used.
  void Init(AvatarToolbarButton* button, Profile* profile);

  // Called by the AvatarToolbarButton to get information about the profile.
  base::string16 GetProfileName() const;
  base::string16 GetShortProfileName() const;
  gfx::Image GetGaiaAccountImage() const;
  gfx::Image GetProfileAvatarImage(gfx::Image gaia_account_image,
                                   int preferred_size) const;

  // Returns the count of incognito or guest windows attached to the profile.
  int GetWindowCount() const;

  AvatarToolbarButton::State GetState() const;

  void ShowHighlightAnimation();
  bool IsHighlightAnimationVisible() const;

  void ShowIdentityAnimation(const gfx::Image& gaia_account_image);

  // Called by the AvatarToolbarButton to notify the delegate about events.
  void NotifyClick();
  void OnMouseExited();
  void OnBlur();
  void OnHighlightChanged();

 private:
  enum class IdentityAnimationState {
    kNotShowing,
    kWaitingForImage,
    kShowingUntilTimeout,
    kShowingUntilNoLongerInUse
  };

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;
  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;

  // IdentityManager::Observer:
  // Needed if the first sync promo account should be displayed.
  void OnUnconsentedPrimaryAccountChanged(
      const CoreAccountInfo& unconsented_primary_account_info) override;
  void OnRefreshTokensLoaded() override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // AvatarButtonErrorControllerDelegate:
  void OnAvatarErrorChanged() override;

  // Initiates showing the identity.
  void OnUserIdentityChanged();

  // Called after the user interacted with the button or after some timeout.
  void OnIdentityAnimationTimeout();
  void MaybeHideIdentityAnimation();
  void HideHighlightAnimation();

  ScopedObserver<ProfileAttributesStorage, ProfileAttributesStorage::Observer>
      profile_observer_{this};
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  AvatarToolbarButton* avatar_toolbar_button_ = nullptr;
  Profile* profile_ = nullptr;
  IdentityAnimationState identity_animation_state_ =
      IdentityAnimationState::kNotShowing;
  bool refresh_tokens_loaded_ = false;
  std::unique_ptr<AvatarButtonErrorController> error_controller_;

  // Whether the avatar highlight animation is visible. The animation is shown
  // when an Autofill datatype is saved. When this is true the avatar button
  // sync paused/error state will be disabled.
  bool highlight_animation_visible_ = false;

  base::WeakPtrFactory<AvatarToolbarButtonDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AvatarToolbarButtonDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_
