// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/sync/sync_service_factory.h"
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

void ShowBubble(Profile* profile,
                views::View* anchor_view,
                ProfileCustomizationBubbleSyncController::Outcome outcome) {
  switch (outcome) {
    case ProfileCustomizationBubbleSyncController::Outcome::kAbort:
      return;
    case ProfileCustomizationBubbleSyncController::Outcome::kShowBubble:
      ProfileCustomizationBubbleView::CreateBubble(profile, anchor_view);
      return;
    case ProfileCustomizationBubbleSyncController::Outcome::kSkipBubble:
      // If the customization bubble is not shown, show the IPH now. Otherwise
      // the IPH will be shown after the customization bubble.
      if (!anchor_view->GetWidget())
        return;
      gfx::NativeWindow window = anchor_view->GetWidget()->GetNativeWindow();
      if (!window || !BrowserView::GetBrowserViewForNativeWindow(window))
        return;
      BrowserView::GetBrowserViewForNativeWindow(window)
          ->MaybeShowProfileSwitchIPH();
      return;
  }
}

}  // namespace

// static
void ProfileCustomizationBubbleSyncController::
    ApplyColorAndShowBubbleWhenNoValueSynced(Profile* profile,
                                             views::View* anchor_view,
                                             SkColor suggested_profile_color) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  // TODO(crbug.com/1213112): A speculative fix, remove if not functional or not
  // needed.
  if (!profile || !anchor_view || !sync_service)
    return;
  // The controller is owned by itself.
  auto* controller = new ProfileCustomizationBubbleSyncController(
      profile, anchor_view, sync_service,
      ThemeServiceFactory::GetForProfile(profile),
      base::BindOnce(&ShowBubble, profile, anchor_view),
      suggested_profile_color);
  controller->Init();
}

// static
void ProfileCustomizationBubbleSyncController::
    ApplyColorAndShowBubbleWhenNoValueSyncedForTesting(
        Profile* profile,
        views::View* anchor_view,
        syncer::SyncService* sync_service,
        ThemeService* theme_service,
        ShowBubbleCallback show_bubble_callback,
        SkColor suggested_profile_color) {
  // The controller is owned by itself.
  auto* controller = new ProfileCustomizationBubbleSyncController(
      profile, anchor_view, sync_service, theme_service,
      std::move(show_bubble_callback), suggested_profile_color);
  controller->Init();
}

// static
bool ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
    Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  return CanSyncStart(sync_service);
}

ProfileCustomizationBubbleSyncController::
    ProfileCustomizationBubbleSyncController(
        Profile* profile,
        views::View* anchor_view,
        syncer::SyncService* sync_service,
        ThemeService* theme_service,
        ShowBubbleCallback show_bubble_callback,
        SkColor suggested_profile_color)
    : sync_service_(sync_service),
      theme_service_(theme_service),
      show_bubble_callback_(std::move(show_bubble_callback)),
      suggested_profile_color_(suggested_profile_color),
      observation_start_time_(base::TimeTicks::Now()) {
  CHECK(profile);
  CHECK(anchor_view);
  CHECK(sync_service_);
  CHECK(theme_service_);
  CHECK(show_bubble_callback_);

  profile_observation_.Observe(profile);
  view_observation_.Observe(anchor_view);
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

  absl::optional<ThemeSyncableService::ThemeSyncState> theme_state =
      theme_service_->GetThemeSyncableService()->GetThemeSyncStartState();
  if (theme_state) {
    // There's enough information to decide whether to show the bubble right on
    // init, finish the flow.
    OnThemeSyncStarted(*theme_state);
    return;
  }

  // Observe the sync service to abort waiting for theme sync if the user hits
  // any error or if custom passphrase is needed.
  sync_observation_.Observe(sync_service_.get());

  theme_observation_.Observe(theme_service_->GetThemeSyncableService());
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

void ProfileCustomizationBubbleSyncController::OnProfileWillBeDestroyed(
    Profile* profile) {
  // This gets called before any keyed services for the profile are destroyed.
  Abort();  // deletes this
}

void ProfileCustomizationBubbleSyncController::OnViewIsDeleting(
    views::View* observed_view) {
  Abort();  // deletes this
}

void ProfileCustomizationBubbleSyncController::
    ApplyDefaultColorAndShowBubble() {
  theme_service_->BuildAutogeneratedThemeFromColor(suggested_profile_color_);
  std::move(show_bubble_callback_).Run(Outcome::kShowBubble);
  delete this;
}

void ProfileCustomizationBubbleSyncController::SkipBubble() {
  std::move(show_bubble_callback_).Run(Outcome::kSkipBubble);
  delete this;
}

void ProfileCustomizationBubbleSyncController::Abort() {
  std::move(show_bubble_callback_).Run(Outcome::kAbort);
  delete this;
}

// Defined in
// chrome/browser/ui/signin/profile_customization_bubble_sync_controller.h
void ApplyProfileColorAndShowCustomizationBubbleWhenNoValueSynced(
    Browser* browser,
    SkColor suggested_profile_color) {
  if (!browser)
    return;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider())
    return;
  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAvatarToolbarButton();
  CHECK(anchor_view);
  ProfileCustomizationBubbleSyncController::
      ApplyColorAndShowBubbleWhenNoValueSynced(browser->profile(), anchor_view,
                                               suggested_profile_color);
}
