// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/sync/service/sync_service.h"

namespace {

void ShowBubble(BrowserWindowInterface* bwi,
                ProfileCustomizationBubbleSyncController::Outcome outcome) {
  switch (outcome) {
    case ProfileCustomizationBubbleSyncController::Outcome::kAbort:
      return;
    case ProfileCustomizationBubbleSyncController::Outcome::kShowBubble:
      bwi->GetFeatures()
          .signin_view_controller()
          ->ShowModalProfileCustomizationDialog();
      return;
    case ProfileCustomizationBubbleSyncController::Outcome::kSkipBubble:
      // If the customization bubble is not shown, show the IPH now. Otherwise
      // the IPH will be shown after the customization bubble.
      if (BrowserView* const browser_view =
              BrowserView::GetBrowserViewForBrowser(bwi)) {
        // Attempts to show first the Supervised user IPH (which has higher
        // priority), then the profile switch IPH. Whether the IPH will show (if
        // all conditions are met) is decided by the IPH framework.
        browser_view->MaybeShowSupervisedUserProfileSignInIPH();
        browser_view->MaybeShowProfileSwitchIPH();
      }

      return;
  }
}

}  // namespace

// static
bool ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
    Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  return ProfileCustomizationSyncedThemeWaiter::CanThemeSyncStart(sync_service);
}

ProfileCustomizationBubbleSyncController::
    ProfileCustomizationBubbleSyncController(BrowserWindowInterface* bwi,
                                             Profile* profile)
    : bwi_(CHECK_DEREF(bwi)), profile_(CHECK_DEREF(profile)) {}

ProfileCustomizationBubbleSyncController::
    ~ProfileCustomizationBubbleSyncController() {
  MaybeInvokeCallback(Outcome::kAbort);
}

void ProfileCustomizationBubbleSyncController::ShowOnSyncFailedOrDefaultTheme(
    SkColor suggested_profile_color) {
  Profile* const profile = base::to_address(profile_);
  ShowOnSyncFailedOrDefaultThemeInternal(
      suggested_profile_color,
      base::BindOnce(&ShowBubble, base::to_address(bwi_)),
      SyncServiceFactory::GetForProfile(profile),
      ThemeServiceFactory::GetForProfile(profile),
      NtpCustomBackgroundServiceFactory::GetForProfile(profile));
}

void ProfileCustomizationBubbleSyncController::
    ShowOnSyncFailedOrDefaultThemeForTesting(
        SkColor suggested_profile_color,
        ShowBubbleCallback show_bubble_callback_testing_override,
        syncer::SyncService* sync_service_testing_override,
        ThemeService* theme_service_testing_override,
        NtpCustomBackgroundService*
            ntp_custom_background_service_testing_override) {
  ShowOnSyncFailedOrDefaultThemeInternal(
      suggested_profile_color, std::move(show_bubble_callback_testing_override),
      sync_service_testing_override, theme_service_testing_override,
      ntp_custom_background_service_testing_override);
}

bool ProfileCustomizationBubbleSyncController::IsWaitingForTheme() const {
  return !!show_bubble_callback_;
}

void ProfileCustomizationBubbleSyncController::
    ShowOnSyncFailedOrDefaultThemeInternal(
        SkColor suggested_profile_color,
        ShowBubbleCallback show_bubble_callback,
        syncer::SyncService* sync_service,
        ThemeService* theme_service,
        NtpCustomBackgroundService* ntp_custom_background_service) {
  // Abort any existing callback before updating the field.
  MaybeInvokeCallback(Outcome::kAbort);

  // TODO(crbug.com/40183503): A speculative fix, remove if not functional or
  // not needed.
  if (!sync_service) {
    return;
  }

  show_bubble_callback_ = std::move(show_bubble_callback);
  theme_waiter_ = std::make_unique<ProfileCustomizationSyncedThemeWaiter>(
      sync_service, theme_service,
      base::BindOnce(
          &ProfileCustomizationBubbleSyncController::OnSyncedThemeReady,
          // base::Unretained() is fine because `this` owns `theme_waiter_`.
          base::Unretained(this), suggested_profile_color, theme_service,
          ntp_custom_background_service));
  theme_waiter_->Run();
}

void ProfileCustomizationBubbleSyncController::OnSyncedThemeReady(
    SkColor suggested_profile_color,
    ThemeService* theme_service,
    NtpCustomBackgroundService* ntp_custom_background_service,
    ProfileCustomizationSyncedThemeWaiter::Outcome outcome) {
  theme_waiter_.reset();
  switch (outcome) {
    case ProfileCustomizationSyncedThemeWaiter::Outcome::kSyncSuccess: {
      const bool using_custom_theme =
          theme_service->GetThemeID() != ThemeHelper::kDefaultThemeID ||
          // Checked separately because grayscale theme also sets
          // kDefaultThemeID.
          theme_service->GetIsGrayscale() ||
          // Checked separately because custom background can be set with
          // kDefaultThemeID.
          ntp_custom_background_service->GetCustomBackground().has_value();
      if (using_custom_theme) {
        MaybeInvokeCallback(Outcome::kSkipBubble);
      } else {
        ApplyDefaultColorAndShowBubble(suggested_profile_color, theme_service);
      }
      break;
    }
    case ProfileCustomizationSyncedThemeWaiter::Outcome::kSyncCannotStart:
      ApplyDefaultColorAndShowBubble(suggested_profile_color, theme_service);
      break;
    case ProfileCustomizationSyncedThemeWaiter::Outcome::
        kSyncPassphraseRequired:
    case ProfileCustomizationSyncedThemeWaiter::Outcome::kTimeout:
      MaybeInvokeCallback(Outcome::kSkipBubble);
      break;
  }
}

void ProfileCustomizationBubbleSyncController::ApplyDefaultColorAndShowBubble(
    SkColor suggested_profile_color,
    ThemeService* theme_service) {
  theme_service->SetUserColorAndBrowserColorVariant(
      suggested_profile_color, ui::mojom::BrowserColorVariant::kTonalSpot);
  MaybeInvokeCallback(Outcome::kShowBubble);
}

void ProfileCustomizationBubbleSyncController::MaybeInvokeCallback(
    Outcome outcome) {
  if (show_bubble_callback_) {
    std::move(show_bubble_callback_).Run(outcome);
  }
}

// Defined in
// chrome/browser/ui/profiles/profile_customization_bubble_sync_controller.h
void ApplyProfileColorAndShowCustomizationBubbleWhenNoValueSynced(
    BrowserWindowInterface* bwi,
    SkColor suggested_profile_color) {
  if (!bwi) {
    return;
  }

  BrowserView* const browser_view = BrowserView::GetBrowserViewForBrowser(bwi);
  if (browser_view && browser_view->toolbar_button_provider()) {
    bwi->GetFeatures()
        .profile_customization_bubble_sync_controller()
        ->ShowOnSyncFailedOrDefaultTheme(suggested_profile_color);
  }
}

// Defined in
// chrome/browser/ui/profiles/profile_customization_bubble_sync_controller.h
bool IsProfileCustomizationBubbleSyncControllerRunning(
    BrowserWindowInterface* bwi) {
  return bwi && bwi->GetFeatures()
                    .profile_customization_bubble_sync_controller()
                    ->IsWaitingForTheme();
}
