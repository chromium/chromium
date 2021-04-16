// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"
#include "components/sync/driver/sync_user_settings.h"

namespace {

bool CanSyncStart(syncer::SyncService* sync_service) {
  if (!sync_service || !sync_service->CanSyncFeatureStart())
    return false;
  if (!sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kThemes)) {
    return false;
  }
  return true;
}

void ShowBubble(Profile* profile, views::View* anchor_view, bool should_show) {
  if (should_show) {
    ProfileCustomizationBubbleView::CreateBubble(profile, anchor_view);
    return;
  }

  // If the customization bubble is not shown, show the IPH now. Otherwise the
  // IPH will be shown after the customization bubble.
  BrowserView::GetBrowserViewForNativeWindow(
      anchor_view->GetWidget()->GetNativeWindow())
      ->MaybeShowProfileSwitchIPH();
}

}  // namespace

// static
void ProfileCustomizationBubbleSyncController::
    ApplyColorAndShowBubbleWhenNoValueSynced(Profile* profile,
                                             views::View* anchor_view,
                                             SkColor suggested_profile_color) {
  // The controller is owned by itself.
  auto* controller = new ProfileCustomizationBubbleSyncController(
      ProfileSyncServiceFactory::GetForProfile(profile),
      ThemeServiceFactory::GetForProfile(profile),
      base::BindOnce(&ShowBubble, profile, anchor_view),
      suggested_profile_color);
  controller->Init();
}

// static
void ProfileCustomizationBubbleSyncController::
    ApplyColorAndShowBubbleWhenNoValueSyncedForTesting(
        syncer::SyncService* sync_service,
        ThemeService* theme_service,
        base::OnceCallback<void(bool)> show_bubble_callback,
        SkColor suggested_profile_color) {
  // The controller is owned by itself.
  auto* controller = new ProfileCustomizationBubbleSyncController(
      sync_service, theme_service, std::move(show_bubble_callback),
      suggested_profile_color);
  controller->Init();
}

// static
bool ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
    Profile* profile) {
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  return CanSyncStart(sync_service);
}

ProfileCustomizationBubbleSyncController::
    ProfileCustomizationBubbleSyncController(
        syncer::SyncService* sync_service,
        ThemeService* theme_service,
        base::OnceCallback<void(bool)> show_bubble_callback,
        SkColor suggested_profile_color)
    : sync_service_(sync_service),
      theme_service_(theme_service),
      show_bubble_callback_(std::move(show_bubble_callback)),
      suggested_profile_color_(suggested_profile_color),
      observation_start_time_(base::TimeTicks::Now()) {
  DCHECK(sync_service_);
  DCHECK(theme_service_);
  DCHECK(show_bubble_callback_);
}

ProfileCustomizationBubbleSyncController::
    ~ProfileCustomizationBubbleSyncController() {
  base::UmaHistogramTimes("Profile.SyncCustomizationBubbleDelay",
                          base::TimeTicks::Now() - observation_start_time_);
}

void ProfileCustomizationBubbleSyncController::Init() {
  if (!CanSyncStart(sync_service_)) {
    ApplyDefaultColorAndShowBubble();
    return;
  }

  theme_observation_.Observe(theme_service_->GetThemeSyncableService());

  // Observe also the sync service to abort waiting for theme sync if the user
  // hits any error or if custom passphrase is needed.
  sync_observation_.Observe(sync_service_);
}

void ProfileCustomizationBubbleSyncController::OnStateChanged(
    syncer::SyncService* sync) {
  // If we figure out sync cannot start (soon), skip the check and show the
  // bubble.
  if (!CanSyncStart(sync)) {
    ApplyDefaultColorAndShowBubble();
    return;
  }

  if (sync->GetUserSettings()->IsPassphraseRequired()) {
    // Keep the default color and do not show the bubble. The reason is that the
    // custom passphrase user may have a color in their sync but Chrome will
    // figure that out later (once the user enter their passphrase) so no prior
    // customization makes sense.
    SkipBubble();
  }
}

void ProfileCustomizationBubbleSyncController::OnThemeSyncStarted(
    ThemeSyncableService::ThemeSyncState state) {
  // Skip the bubble (and not use the default color) if the user got a custom
  // value from sync (that is either already applied as a custom theme or
  // triggered a custom theme installation).
  const bool using_custom_theme = !theme_service_->UsingDefaultTheme() &&
                                  !theme_service_->UsingSystemTheme();
  const bool installing_custom_theme =
      state ==
      ThemeSyncableService::ThemeSyncState::kWaitingForExtensionInstallation;
  if (using_custom_theme || installing_custom_theme) {
    SkipBubble();
    return;
  }
  ApplyDefaultColorAndShowBubble();
}

void ProfileCustomizationBubbleSyncController::
    ApplyDefaultColorAndShowBubble() {
  theme_service_->BuildAutogeneratedThemeFromColor(suggested_profile_color_);
  std::move(show_bubble_callback_).Run(true);
  delete this;
}

void ProfileCustomizationBubbleSyncController::SkipBubble() {
  std::move(show_bubble_callback_).Run(false);
  delete this;
}

// Defined in
// chrome/browser/ui/signin/profile_customization_bubble_sync_controller.h
void ApplyProfileColorAndShowCustomizationBubbleWhenNoValueSynced(
    Browser* browser,
    SkColor suggested_profile_color) {
  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAvatarToolbarButton();
  DCHECK(anchor_view);
  ProfileCustomizationBubbleSyncController::
      ApplyColorAndShowBubbleWhenNoValueSynced(browser->profile(), anchor_view,
                                               suggested_profile_color);
}
