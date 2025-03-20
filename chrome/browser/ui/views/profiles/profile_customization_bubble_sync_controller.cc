// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/base/mojom/themes.mojom.h"

namespace {

const void* const kBrowserUserDataKey = &kBrowserUserDataKey;

void ShowBubble(Browser* browser,
                ProfileCustomizationBubbleSyncController::Outcome outcome) {
  switch (outcome) {
    case ProfileCustomizationBubbleSyncController::Outcome::kAbort:
      return;
    case ProfileCustomizationBubbleSyncController::Outcome::kShowBubble:
      browser->signin_view_controller()->ShowModalProfileCustomizationDialog();
      return;
    case ProfileCustomizationBubbleSyncController::Outcome::kSkipBubble:
      // If the customization bubble is not shown, show the IPH now. Otherwise
      // the IPH will be shown after the customization bubble.
      BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser);
      if (!browser_view) {
        return;
      }
      // Attempts to show first the Supervised user IPH (which has higher
      // priority), then the profile switch IPH. Whether the IPH will show (if
      // all conditions are met) is decided by the IPH framework.
      browser_view->MaybeShowSupervisedUserProfileSignInIPH();
      browser_view->MaybeShowProfileSwitchIPH();
      return;
  }
}

ProfileCustomizationBubbleSyncController* GetCurrentControllerIfExists(
    Browser* browser) {
  base::SupportsUserData::Data* data =
      browser->GetUserData(kBrowserUserDataKey);
  if (!data) {
    return nullptr;
  }

  return static_cast<ProfileCustomizationBubbleSyncController*>(data);
}

}  // namespace

// static
void ProfileCustomizationBubbleSyncController::
    ApplyColorAndShowBubbleWhenNoValueSynced(Browser* browser,
                                             SkColor suggested_profile_color) {
  DCHECK(browser);
  Profile* profile = browser->profile();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  // TODO(crbug.com/40183503): A speculative fix, remove if not functional or
  // not needed.
  if (!sync_service) {
    return;
  }

  auto controller =
      base::WrapUnique(new ProfileCustomizationBubbleSyncController(
          browser, sync_service, ThemeServiceFactory::GetForProfile(profile),
          base::BindOnce(&ShowBubble, browser), suggested_profile_color));
  SetCurrentControllerAndInit(std::move(controller));
}

// static
void ProfileCustomizationBubbleSyncController::
    ApplyColorAndShowBubbleWhenNoValueSyncedForTesting(
        Browser* browser,
        syncer::SyncService* sync_service,
        ThemeService* theme_service,
        ShowBubbleCallback show_bubble_callback,
        SkColor suggested_profile_color) {
  auto controller =
      base::WrapUnique(new ProfileCustomizationBubbleSyncController(
          browser, sync_service, theme_service, std::move(show_bubble_callback),
          suggested_profile_color));
  SetCurrentControllerAndInit(std::move(controller));
}

// static
bool ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
    Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  return ProfileCustomizationSyncedThemeWaiter::CanThemeSyncStart(sync_service);
}

// static
void ProfileCustomizationBubbleSyncController::SetCurrentControllerAndInit(
    std::unique_ptr<ProfileCustomizationBubbleSyncController> controller) {
  Browser* browser = controller->browser_;
  if (ProfileCustomizationBubbleSyncController* old_controller =
          GetCurrentControllerIfExists(browser);
      old_controller) {
    old_controller->InvokeCallbackAndDeleteItself(Outcome::kAbort);
  }

  ProfileCustomizationBubbleSyncController* controller_raw = controller.get();
  browser->SetUserData(kBrowserUserDataKey, std::move(controller));
  controller_raw->Init();
}

ProfileCustomizationBubbleSyncController::
    ProfileCustomizationBubbleSyncController(
        Browser* browser,
        syncer::SyncService* sync_service,
        ThemeService* theme_service,
        ShowBubbleCallback show_bubble_callback,
        SkColor suggested_profile_color)
    : browser_(browser),
      theme_service_(theme_service),
      show_bubble_callback_(std::move(show_bubble_callback)),
      suggested_profile_color_(suggested_profile_color) {
  CHECK(browser);
  CHECK(sync_service);
  CHECK(theme_service_);
  CHECK(show_bubble_callback_);

  theme_waiter_ = std::make_unique<ProfileCustomizationSyncedThemeWaiter>(
      sync_service, theme_service_,
      base::BindOnce(
          &ProfileCustomizationBubbleSyncController::OnSyncedThemeReady,
          // base::Unretained() is fine here because `this` owns
          // `theme_waiter_`.
          base::Unretained(this)));
}

ProfileCustomizationBubbleSyncController::
    ~ProfileCustomizationBubbleSyncController() {
  if (show_bubble_callback_) {
    std::move(show_bubble_callback_).Run(Outcome::kAbort);
  }
}

void ProfileCustomizationBubbleSyncController::Init() {
  // Verify that the user data has been set before calling `Init()`.
  CHECK_EQ(browser_->GetUserData(kBrowserUserDataKey), this);
  theme_waiter_->Run();
}

void ProfileCustomizationBubbleSyncController::OnSyncedThemeReady(
    ProfileCustomizationSyncedThemeWaiter::Outcome outcome) {
  theme_waiter_.reset();
  switch (outcome) {
    case ProfileCustomizationSyncedThemeWaiter::Outcome::kSyncSuccess: {
      bool using_custom_theme = !theme_service_->UsingDefaultTheme() &&
                                !theme_service_->UsingSystemTheme();
      if (using_custom_theme) {
        InvokeCallbackAndDeleteItself(Outcome::kSkipBubble);
      } else {
        ApplyDefaultColorAndShowBubble();
      }
      break;
    }
    case ProfileCustomizationSyncedThemeWaiter::Outcome::kSyncCannotStart:
      ApplyDefaultColorAndShowBubble();
      break;
    case ProfileCustomizationSyncedThemeWaiter::Outcome::
        kSyncPassphraseRequired:
    case ProfileCustomizationSyncedThemeWaiter::Outcome::kTimeout:
      InvokeCallbackAndDeleteItself(Outcome::kSkipBubble);
      break;
  }
}

void ProfileCustomizationBubbleSyncController::
    ApplyDefaultColorAndShowBubble() {
  theme_service_->SetUserColorAndBrowserColorVariant(
      suggested_profile_color_, ui::mojom::BrowserColorVariant::kTonalSpot);
  InvokeCallbackAndDeleteItself(Outcome::kShowBubble);
}

void ProfileCustomizationBubbleSyncController::InvokeCallbackAndDeleteItself(
    Outcome outcome) {
  std::move(show_bubble_callback_).Run(outcome);
  CHECK_EQ(browser_->GetUserData(kBrowserUserDataKey), this);
  browser_->RemoveUserData(kBrowserUserDataKey);  // deletes `this`
}

// Defined in
// chrome/browser/ui/profiles/profile_customization_bubble_sync_controller.h
void ApplyProfileColorAndShowCustomizationBubbleWhenNoValueSynced(
    Browser* browser,
    SkColor suggested_profile_color) {
  if (!browser) {
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return;
  }
  ProfileCustomizationBubbleSyncController::
      ApplyColorAndShowBubbleWhenNoValueSynced(browser,
                                               suggested_profile_color);
}

// Defined in
// chrome/browser/ui/profiles/profile_customization_bubble_sync_controller.h
bool IsProfileCustomizationBubbleSyncControllerRunning(Browser* browser) {
  return GetCurrentControllerIfExists(browser) != nullptr;
}
