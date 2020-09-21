// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button_delegate.h"

#include "base/check_op.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace {

constexpr base::TimeDelta kIdentityAnimationDuration =
    base::TimeDelta::FromSeconds(3);

constexpr base::TimeDelta kAvatarHighlightAnimationDuration =
    base::TimeDelta::FromSeconds(2);

ProfileAttributesStorage& GetProfileAttributesStorage() {
  return g_browser_process->profile_manager()->GetProfileAttributesStorage();
}

ProfileAttributesEntry* GetProfileAttributesEntry(Profile* profile) {
  ProfileAttributesEntry* entry;
  if (!GetProfileAttributesStorage().GetProfileAttributesWithPath(
          profile->GetPath(), &entry)) {
    return nullptr;
  }
  return entry;
}

bool IsGenericProfile(const ProfileAttributesEntry& entry) {
  // If the profile is using the placeholder avatar, fall back on the generic
  // profile's themeable vector icon instead.
  if (entry.GetAvatarIconIndex() == profiles::GetPlaceholderAvatarIndex())
    return true;

  return entry.GetAvatarIconIndex() == 0 &&
         GetProfileAttributesStorage().GetNumberOfProfiles() == 1;
}

// Returns the avatar image for the current profile. May be called only in
// "normal" states where the user is guaranteed to have an avatar image (i.e.
// not kGuestSession and not kIncognitoProfile).
gfx::Image GetAvatarImage(Profile* profile,
                          const gfx::Image& user_identity_image,
                          int preferred_size) {
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile);
  if (!entry) {  // This can happen if the user deletes the current profile.
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }

  // TODO(crbug.com/1012179): it should suffice to call entry->GetAvatarIcon().
  // For this to work well, this class needs to observe ProfileAttributesStorage
  // instead of (or on top of) IdentityManager. Only then we can rely on |entry|
  // being up to date (as the storage also observes IdentityManager so there's
  // no guarantee on the order of notifications).
  if (entry->IsUsingGAIAPicture() && entry->GetGAIAPicture())
    return *entry->GetGAIAPicture();

  // Show |user_identity_image| when the following conditions are satisfied:
  //  - the user is migrated to Dice
  //  - the user isn't syncing
  //  - the profile icon wasn't explicitly changed
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!user_identity_image.IsEmpty() &&
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile) &&
      !identity_manager->HasPrimaryAccount() && entry->IsUsingDefaultAvatar()) {
    return user_identity_image;
  }

  return entry->GetAvatarIcon(preferred_size);
}

}  // namespace

AvatarToolbarButtonDelegate::AvatarToolbarButtonDelegate() = default;

AvatarToolbarButtonDelegate::~AvatarToolbarButtonDelegate() {
  BrowserList::RemoveObserver(this);
}

void AvatarToolbarButtonDelegate::Init(AvatarToolbarButton* button,
                                       Profile* profile) {
  avatar_toolbar_button_ = button;
  profile_ = profile;
  error_controller_ =
      std::make_unique<AvatarButtonErrorController>(this, profile_);
  profile_observer_.Add(&GetProfileAttributesStorage());
  AvatarToolbarButton::State state = GetState();
  if (state == AvatarToolbarButton::State::kIncognitoProfile ||
      state == AvatarToolbarButton::State::kGuestSession) {
    BrowserList::AddObserver(this);
  } else if (state != AvatarToolbarButton::State::kGuestSession) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_);
    identity_manager_observer_.Add(identity_manager);

    if (identity_manager->AreRefreshTokensLoaded())
      OnRefreshTokensLoaded();
  }

#if defined(OS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(chromeos::features::kAvatarToolbarButton)) {
    // On CrOS this button should only show as badging for Incognito and Guest
    // sessions. It's only enabled for Incognito where a menu is available for
    // closing all Incognito windows.
    avatar_toolbar_button_->SetEnabled(
        state == AvatarToolbarButton::State::kIncognitoProfile);
  }
#endif  // !defined(OS_CHROMEOS)
}

base::string16 AvatarToolbarButtonDelegate::GetProfileName() const {
  DCHECK_NE(GetState(), AvatarToolbarButton::State::kIncognitoProfile);
  return profiles::GetAvatarNameForProfile(profile_->GetPath());
}

base::string16 AvatarToolbarButtonDelegate::GetShortProfileName() const {
  return signin_ui_util::GetShortProfileIdentityToDisplay(
      *GetProfileAttributesEntry(profile_), profile_);
}

gfx::Image AvatarToolbarButtonDelegate::GetGaiaAccountImage() const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kNotRequired)) {
    base::Optional<AccountInfo> account_info =
        identity_manager
            ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
                identity_manager->GetPrimaryAccountId(
                    signin::ConsentLevel::kNotRequired));
    if (account_info.has_value())
      return account_info->account_image;
  }
  return gfx::Image();
}

gfx::Image AvatarToolbarButtonDelegate::GetProfileAvatarImage(
    gfx::Image gaia_account_image,
    int preferred_size) const {
  return GetAvatarImage(profile_, gaia_account_image, preferred_size);
}

int AvatarToolbarButtonDelegate::GetWindowCount() const {
  if (profile_->IsGuestSession())
    return BrowserList::GetGuestBrowserCount();
  DCHECK(profile_->IsOffTheRecord());
  return BrowserList::GetOffTheRecordBrowsersActiveForProfile(profile_);
}

AvatarToolbarButton::State AvatarToolbarButtonDelegate::GetState() const {
  if (profile_->IsGuestSession())
    return AvatarToolbarButton::State::kGuestSession;

  // Return |kIncognitoProfile| state for all OffTheRecord profile types except
  // guest mode.
  if (profile_->IsOffTheRecord())
    return AvatarToolbarButton::State::kIncognitoProfile;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  if (!entry ||  // This can happen if the user deletes the current profile.
      (!identity_manager->HasPrimaryAccount(
           signin::ConsentLevel::kNotRequired) &&
       IsGenericProfile(*entry))) {
    return AvatarToolbarButton::State::kGenericProfile;
  }

  if (identity_animation_state_ ==
          IdentityAnimationState::kShowingUntilTimeout ||
      identity_animation_state_ ==
          IdentityAnimationState::kShowingUntilNoLongerInUse) {
    return AvatarToolbarButton::State::kAnimatedUserIdentity;
  }

  if (identity_manager->HasPrimaryAccount() &&
      ProfileSyncServiceFactory::IsSyncAllowed(profile_) &&
      error_controller_->HasAvatarError()) {
    const sync_ui_util::AvatarSyncErrorType error =
        sync_ui_util::GetAvatarSyncErrorType(profile_);

    // When DICE is enabled and the error is an auth error, the sync-paused
    // icon is shown.
    if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_) &&
        error == sync_ui_util::AUTH_ERROR) {
      return AvatarToolbarButton::State::kSyncPaused;
    }

    if (error == sync_ui_util::TRUSTED_VAULT_KEY_MISSING_FOR_PASSWORDS_ERROR) {
      return AvatarToolbarButton::State::kPasswordsOnlySyncError;
    }

    return AvatarToolbarButton::State::kSyncError;
  }

  return AvatarToolbarButton::State::kNormal;
}

void AvatarToolbarButtonDelegate::ShowHighlightAnimation() {
  signin_ui_util::RecordAvatarIconHighlighted(profile_);
  highlight_animation_visible_ = true;
  DCHECK_NE(GetState(), AvatarToolbarButton::State::kIncognitoProfile);
  DCHECK_NE(GetState(), AvatarToolbarButton::State::kGuestSession);
  avatar_toolbar_button_->UpdateText();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AvatarToolbarButtonDelegate::HideHighlightAnimation,
                     weak_ptr_factory_.GetWeakPtr()),
      kAvatarHighlightAnimationDuration);
}

bool AvatarToolbarButtonDelegate::IsHighlightAnimationVisible() const {
  return highlight_animation_visible_;
}

void AvatarToolbarButtonDelegate::ShowIdentityAnimation(
    const gfx::Image& gaia_account_image) {
  // TODO(crbug.com/990286): Get rid of this logic completely when we cache the
  // Google account image in the profile cache and thus it is always available.
  if (identity_animation_state_ != IdentityAnimationState::kWaitingForImage ||
      gaia_account_image.IsEmpty()) {
    return;
  }

  // Check that the user is still signed in. See https://crbug.com/1025674
  CoreAccountInfo user_identity =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kNotRequired);
  if (user_identity.IsEmpty()) {
    identity_animation_state_ = IdentityAnimationState::kNotShowing;
    return;
  }

  identity_animation_state_ = IdentityAnimationState::kShowingUntilTimeout;
  avatar_toolbar_button_->UpdateText();

  // Hide the pill after a while.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AvatarToolbarButtonDelegate::OnIdentityAnimationTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      kIdentityAnimationDuration);
}

void AvatarToolbarButtonDelegate::NotifyClick() {
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::OnMouseExited() {
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::OnBlur() {
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::OnHighlightChanged() {
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::OnBrowserAdded(Browser* browser) {
  avatar_toolbar_button_->UpdateIcon();
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::OnBrowserRemoved(Browser* browser) {
  avatar_toolbar_button_->UpdateIcon();
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::OnProfileAdded(
    const base::FilePath& profile_path) {
  // Adding any profile changes the profile count, we might go from showing a
  // generic avatar button to profile pictures here. Update icon accordingly.
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  // Removing a profile changes the profile count, we might go from showing
  // per-profile icons back to a generic avatar icon. Update icon accordingly.
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
  if (unconsented_primary_account_info.IsEmpty())
    return;
  OnUserIdentityChanged();
}

void AvatarToolbarButtonDelegate::OnRefreshTokensLoaded() {
  if (refresh_tokens_loaded_) {
    // This is possible, if |AvatarToolbarButtonDelegate::Init| is called within
    // the loop in |IdentityManager::OnRefreshTokensLoaded()| to notify
    // observers. In that case, |OnRefreshTokensLoaded| will be called twice,
    // once from |AvatarToolbarButtonDelegate::Init| and another time from the
    // |IdentityManager|. This happens for new signed in profiles.
    // See https://crbug.com/1035480
    return;
  }

  refresh_tokens_loaded_ = true;
  if (!signin_ui_util::ShouldShowAnimatedIdentityOnOpeningWindow(
          GetProfileAttributesStorage(), profile_)) {
    return;
  }
  CoreAccountInfo account =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kNotRequired);
  if (account.IsEmpty())
    return;
  OnUserIdentityChanged();
}

void AvatarToolbarButtonDelegate::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnAvatarErrorChanged() {
  avatar_toolbar_button_->UpdateIcon();
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::OnUserIdentityChanged() {
  signin_ui_util::RecordAnimatedIdentityTriggered(profile_);
  identity_animation_state_ = IdentityAnimationState::kWaitingForImage;
  // If we already have a gaia image, the pill will be immediately displayed by
  // UpdateIcon().
  avatar_toolbar_button_->UpdateIcon();
}

void AvatarToolbarButtonDelegate::OnIdentityAnimationTimeout() {
  DCHECK_EQ(identity_animation_state_,
            IdentityAnimationState::kShowingUntilTimeout);
  identity_animation_state_ =
      IdentityAnimationState::kShowingUntilNoLongerInUse;
  MaybeHideIdentityAnimation();
}

void AvatarToolbarButtonDelegate::MaybeHideIdentityAnimation() {
  // No-op if not showing or if the timeout hasn't passed, yet.
  if (identity_animation_state_ !=
      IdentityAnimationState::kShowingUntilNoLongerInUse) {
    return;
  }

  // Keep identity visible if this button is in use (hovered or has focus) or
  // if its parent is in use (which makes it highlighted). We should not move
  // things around when the user wants to click on |this| or another button in
  // the parent.
  if (avatar_toolbar_button_->IsMouseHovered() ||
      avatar_toolbar_button_->HasFocus()) {
    return;
  }

  identity_animation_state_ = IdentityAnimationState::kNotShowing;
  // Update the text to the pre-shown state. This also makes sure that we now
  // reflect changes that happened while the identity pill was shown.
  avatar_toolbar_button_->UpdateText();
}

void AvatarToolbarButtonDelegate::HideHighlightAnimation() {
  DCHECK_NE(GetState(), AvatarToolbarButton::State::kIncognitoProfile);
  DCHECK_NE(GetState(), AvatarToolbarButton::State::kGuestSession);
  highlight_animation_visible_ = false;
  avatar_toolbar_button_->UpdateText();
  avatar_toolbar_button_->NotifyHighlightAnimationFinished();
}
