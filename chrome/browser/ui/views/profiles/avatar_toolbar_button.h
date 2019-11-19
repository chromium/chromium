// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/avatar_button_error_controller.h"
#include "chrome/browser/ui/avatar_button_error_controller_delegate.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/events/event.h"

class Browser;

class AvatarToolbarButton : public ToolbarButton,
                            public AvatarButtonErrorControllerDelegate,
                            public BrowserListObserver,
                            public ProfileAttributesStorage::Observer,
                            public signin::IdentityManager::Observer,
                            public ui::MaterialDesignControllerObserver,
                            ToolbarIconContainerView::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnAvatarHighlightAnimationFinished() = 0;
  };

  // TODO(crbug.com/922525): Remove this constructor when this button always has
  // ToolbarIconContainerView as a parent.
  explicit AvatarToolbarButton(Browser* browser);
  AvatarToolbarButton(Browser* browser, ToolbarIconContainerView* parent);
  ~AvatarToolbarButton() override;

  void UpdateIcon();
  void UpdateText();
  void ShowAvatarHighlightAnimation();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // views::View:
  const char* GetClassName() const override;

  static const char kAvatarToolbarButtonClassName[];

 private:
  FRIEND_TEST_ALL_PREFIXES(AvatarToolbarButtonTest,
                           HighlightMeetsMinimumContrast);

  // States of the button ordered in priority of getting displayed.
  enum class State {
    kIncognitoProfile,
    kGuestSession,
    kGenericProfile,
    kAnimatedUserIdentity,
    kSyncPaused,
    kSyncError,
    kNormal
  };

  enum class IdentityAnimationState {
    kNotShowing,
    kWaitingForImage,
    kShowingUntilTimeout,
    kShowingUntilNoLongerInUse
  };

  // ToolbarButton:
  void NotifyClick(const ui::Event& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnBlur() override;
  void OnThemeChanged() override;
  void AddedToWidget() override;

  // AvatarButtonErrorControllerDelegate:
  void OnAvatarErrorChanged() override;

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

  // ui::MaterialDesignControllerObserver:
  void OnTouchUiChanged() override;

  // ToolbarIconContainerView::Observer:
  void OnHighlightChanged() override;

  void ShowIdentityAnimation();
  void OnIdentityAnimationTimeout();
  void MaybeHideIdentityAnimation();

  base::string16 GetAvatarTooltipText() const;
  base::string16 GetProfileName() const;
  gfx::ImageSkia GetAvatarIcon(const gfx::Image& user_identity_image) const;

  // Returns the image of the unconsented primary account (if exists and already
  // loaded), otherwise empty.
  gfx::Image GetUserIdentityImage() const;

  State GetState() const;

  void SetInsets();

  // Initiates showing the identity |user_identity| (if non-empty).
  void OnUserIdentityChanged(const CoreAccountInfo& user_identity,
                             const base::Feature& triggering_feature);

  void ShowHighlightAnimation();
  void HideHighlightAnimation();

#if !defined(OS_CHROMEOS)
  AvatarButtonErrorController error_controller_;
#endif  // !defined(OS_CHROMEOS)

  Browser* const browser_;
  Profile* const profile_;
  ToolbarIconContainerView* const parent_;

  // Whether the avatar highlight animation is visible. The animation is shown
  // when an Autofill datatype is saved. When this is true the avatar button
  // sync paused/error state will be disabled.
  bool highlight_animation_visible_ = false;

  IdentityAnimationState identity_animation_state_ =
      IdentityAnimationState::kNotShowing;

  ScopedObserver<ProfileAttributesStorage, ProfileAttributesStorage::Observer>
      profile_observer_{this};
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  ScopedObserver<ui::MaterialDesignController,
                 ui::MaterialDesignControllerObserver>
      md_observer_{this};

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<AvatarToolbarButton> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AvatarToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_H_
