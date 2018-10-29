// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button_border.h"

namespace {

ProfileAttributesEntry* GetProfileAttributesEntry(Profile* profile) {
  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()
           ->GetProfileAttributesStorage()
           .GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    return nullptr;
  }
  return entry;
}

}  // namespace

AvatarToolbarButton::AvatarToolbarButton(Browser* browser)
    : ToolbarButton(nullptr),
      browser_(browser),
      profile_(browser_->profile()),
#if !defined(OS_CHROMEOS)
      error_controller_(this, profile_),
#endif  // !defined(OS_CHROMEOS)
      browser_list_observer_(this),
      profile_observer_(this),
      cookie_manager_service_observer_(this),
      account_tracker_service_observer_(this) {

  if (IsIncognitoCounterActive())
    browser_list_observer_.Add(BrowserList::GetInstance());

  profile_observer_.Add(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());

  if (!IsIncognito() && !profile_->IsGuestSession()) {
    cookie_manager_service_observer_.Add(
        GaiaCookieManagerServiceFactory::GetForProfile(profile_));
    account_tracker_service_observer_.Add(
        AccountTrackerServiceFactory::GetForProfile(profile_));
  }

  SetInsets();

  // Activate on press for left-mouse-button only to mimic other MenuButtons
  // without drag-drop actions (specifically the adjacent browser menu).
  set_notify_action(Button::NOTIFY_ON_PRESS);
  set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON);

  set_tag(IDC_SHOW_AVATAR_MENU);

  // The avatar should not flip with RTL UI. This does not affect text rendering
  // and LabelButton image/label placement is still flipped like usual.
  EnableCanvasFlippingForRTLUI(false);

  Init();

#if defined(OS_CHROMEOS)
  // On CrOS the avatar toolbar button should only show as badging for Incognito
  // and Guest sessions. It should not be instantiated for regular profiles and
  // it should not be enabled as there's no profile switcher to trigger / show,
  // unless incognito window counter is available.
  DCHECK(IsIncognito() || profile_->IsGuestSession());
  SetEnabled(IsIncognitoCounterActive());
#else
  // The profile switcher is only available outside incognito or if incognito
  // window counter is enabled.
  SetEnabled(!IsIncognito() || IsIncognitoCounterActive());
#endif  // !defined(OS_CHROMEOS)

  // Set initial text and tooltip. UpdateIcon() needs to be called from the
  // outside as GetThemeProvider() is not available until the button is added to
  // ToolbarView's hierarchy.
  UpdateText();

  md_observer_.Add(ui::MaterialDesignController::GetInstance());
}

AvatarToolbarButton::~AvatarToolbarButton() {}

void AvatarToolbarButton::UpdateIcon() {
  SetImage(views::Button::STATE_NORMAL, GetAvatarIcon());
}

void AvatarToolbarButton::UpdateText() {
  base::Optional<SkColor> color;
  base::string16 text;

  const SyncState sync_state = GetSyncState();

  if (IsIncognitoCounterActive()) {
    const int incognito_window_count =
        BrowserList::GetIncognitoSessionsActiveForProfile(profile_);
    if (incognito_window_count > 1) {
      text = base::IntToString16(incognito_window_count);
      // TODO(http://crbug.com/896235): Update to select from theme colors and
      // use GetColorWithMinimumContrast to guarantee readability.
      color = gfx::kGoogleGrey900;
    }
  } else if (sync_state == SyncState::kError) {
    color = gfx::kGoogleRed600;
    text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR);
  } else if (sync_state == SyncState::kPaused) {
    color = gfx::kGoogleBlue600;
    text = l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED);
  }

  SetHighlightColor(color);
  SetText(text);

  SetTooltipText(GetAvatarTooltipText());
}

void AvatarToolbarButton::NotifyClick(const ui::Event& event) {
  Button::NotifyClick(event);
  // TODO(bsep): Other toolbar buttons have ToolbarView as a listener and let it
  // call ExecuteCommandWithDisposition on their behalf. Unfortunately, it's not
  // possible to plumb IsKeyEvent through, so this has to be a special case.
  browser_->window()->ShowAvatarBubbleFromAvatarButton(
      BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT, signin::ManageAccountsParams(),
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
      event.IsKeyEvent());
}

void AvatarToolbarButton::OnAvatarErrorChanged() {
  UpdateIcon();
  UpdateText();
}

void AvatarToolbarButton::OnBrowserAdded(Browser* browser) {
  UpdateIcon();
  UpdateText();
}

void AvatarToolbarButton::OnBrowserRemoved(Browser* browser) {
  UpdateIcon();
  UpdateText();
}

void AvatarToolbarButton::OnProfileAdded(const base::FilePath& profile_path) {
  // Adding any profile changes the profile count, we might go from showing a
  // generic avatar button to profile pictures here. Update icon accordingly.
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  // Removing a profile changes the profile count, we might go from showing
  // per-profile icons back to a generic avatar icon. Update icon accordingly.
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  UpdateIcon();
}

void AvatarToolbarButton::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  UpdateText();
}

void AvatarToolbarButton::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  UpdateIcon();
}

void AvatarToolbarButton::OnAccountImageUpdated(const std::string& account_id,
                                                const gfx::Image& image) {
  UpdateIcon();
}

void AvatarToolbarButton::OnAccountRemoved(const AccountInfo& info) {
  UpdateIcon();
}

void AvatarToolbarButton::OnTouchUiChanged() {
  SetInsets();
  PreferredSizeChanged();
}

bool AvatarToolbarButton::IsIncognito() const {
  return profile_->IsOffTheRecord() && !profile_->IsGuestSession();
}

bool AvatarToolbarButton::IsIncognitoCounterActive() const {
  return IsIncognito() &&
         base::FeatureList::IsEnabled(features::kEnableIncognitoWindowCounter);
}

bool AvatarToolbarButton::ShouldShowGenericIcon() const {
  // This function should only be used for regular profiles. Guest and Incognito
  // sessions should be handled separately and never call this function.
  DCHECK(!profile_->IsGuestSession());
  DCHECK(!profile_->IsOffTheRecord());
#if !defined(OS_CHROMEOS)
  if (!signin_ui_util::GetAccountsForDicePromos(profile_).empty())
    return false;
#endif  // !defined(OS_CHROMEOS)

  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  if (!entry) {
    // This can happen if the user deletes the current profile.
    return true;
  }
  return entry->IsUsingDefaultAvatar() &&
         g_browser_process->profile_manager()
                 ->GetProfileAttributesStorage()
                 .GetNumberOfProfiles() == 1 &&
         !IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount();
}

base::string16 AvatarToolbarButton::GetAvatarTooltipText() const {
  if (IsIncognito())
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_INCOGNITO_TOOLTIP);

  if (profile_->IsGuestSession())
    return l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);

  if (ShouldShowGenericIcon())
    return l10n_util::GetStringUTF16(IDS_GENERIC_USER_AVATAR_LABEL);

  const base::string16 profile_name =
      profiles::GetAvatarNameForProfile(profile_->GetPath());
  switch (GetSyncState()) {
    case SyncState::kNormal:
      return profile_name;
    case SyncState::kPaused:
      return l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED_TOOLTIP,
                                        profile_name);
    case SyncState::kError:
      return l10n_util::GetStringFUTF16(IDS_AVATAR_BUTTON_SYNC_ERROR_TOOLTIP,
                                        profile_name);
  }

  NOTREACHED();
  return base::string16();
}

gfx::ImageSkia AvatarToolbarButton::GetAvatarIcon() const {
  const int icon_size = ui::MaterialDesignController::touch_ui() ? 24 : 20;

  SkColor icon_color;
  if (IsIncognitoCounterActive() &&
      BrowserList::GetIncognitoSessionsActiveForProfile(profile_) > 1) {
    // TODO(http://crbug.com/896235): Update to select from theme colors.
    icon_color = gfx::kGoogleGrey900;
  } else {
    icon_color = GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  }

  if (IsIncognito())
    return gfx::CreateVectorIcon(kIncognitoIcon, icon_size, icon_color);

  if (profile_->IsGuestSession())
    return gfx::CreateVectorIcon(kUserMenuGuestIcon, icon_size, icon_color);

  gfx::Image avatar_icon;
  if (!ShouldShowGenericIcon())
    avatar_icon = GetIconImageFromProfile();

  if (!avatar_icon.IsEmpty()) {
    return profiles::GetSizedAvatarIcon(avatar_icon, true, icon_size, icon_size,
                                        profiles::SHAPE_CIRCLE)
        .AsImageSkia();
  }

  return gfx::CreateVectorIcon(kUserAccountAvatarIcon, icon_size, icon_color);
}

gfx::Image AvatarToolbarButton::GetIconImageFromProfile() const {
  ProfileAttributesEntry* entry = GetProfileAttributesEntry(profile_);
  if (!entry) {
    // This can happen if the user deletes the current profile.
    return gfx::Image();
  }

  // If there is a GAIA image available, try to use that.
  if (entry->IsUsingGAIAPicture()) {
    // TODO(chengx): The GetGAIAPicture API call will trigger an async image
    // load from disk if it has not been loaded. This is non-obvious and
    // dependency should be avoided. We should come with a better idea to handle
    // this.
    const gfx::Image* gaia_image = entry->GetGAIAPicture();

    if (gaia_image)
      return *gaia_image;
    return gfx::Image();
  }

#if !defined(OS_CHROMEOS)
  // Try to show the first account icon of the sync promo when the following
  // conditions are satisfied:
  //  - the user is migrated to Dice
  //  - the user isn't signed in
  //  - the profile icon wasn't explicitly changed
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_) &&
      !IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount() &&
      entry->IsUsingDefaultAvatar()) {
    std::vector<AccountInfo> promo_accounts =
        signin_ui_util::GetAccountsForDicePromos(profile_);
    if (!promo_accounts.empty()) {
      return AccountTrackerServiceFactory::GetForProfile(profile_)
          ->GetAccountImage(promo_accounts[0].account_id);
    }
  }
#endif  // !defined(OS_CHROMEOS)

  return entry->GetAvatarIcon();
}

AvatarToolbarButton::SyncState AvatarToolbarButton::GetSyncState() const {
#if !defined(OS_CHROMEOS)
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager && identity_manager->HasPrimaryAccount() &&
      profile_->IsSyncAllowed() && error_controller_.HasAvatarError()) {
    // When DICE is enabled and the error is an auth error, the sync-paused
    // icon is shown.
    int unused;
    const bool should_show_sync_paused_ui =
        AccountConsistencyModeManager::IsDiceEnabledForProfile(profile_) &&
        // TODO(http://crbug.com/890796): Migrate to passing an IdentityManager
        sync_ui_util::GetMessagesForAvatarSyncError(
            profile_, *SigninManagerFactory::GetForProfile(profile_), &unused,
            &unused) == sync_ui_util::AUTH_ERROR;
    return should_show_sync_paused_ui ? SyncState::kPaused : SyncState::kError;
  }
#endif  // !defined(OS_CHROMEOS)
  return SyncState::kNormal;
}

void AvatarToolbarButton::SetInsets() {
  // In non-touch mode we use a larger-than-normal icon size for avatars as 16dp
  // is hard to read for user avatars, so we need to set corresponding insets.
  SetLayoutInsetDelta(
      gfx::Insets(ui::MaterialDesignController::touch_ui() ? 0 : -2));
}
