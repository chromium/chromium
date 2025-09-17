// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_SYNC_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_SYNC_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/ui/profiles/profile_customization_synced_theme_waiter.h"
#include "third_party/skia/include/core/SkColor.h"

class BrowserWindowInterface;
class Profile;
class ThemeService;

namespace syncer {
class SyncService;
}

// Helper class for logic to show / delay showing the profile customization
// bubble. Owned by a Browser.
class ProfileCustomizationBubbleSyncController {
 public:
  enum class Outcome {
    kShowBubble,
    kSkipBubble,
    // The browser is being destroyed.
    kAbort
  };
  using ShowBubbleCallback = base::OnceCallback<void(Outcome outcome)>;

  ProfileCustomizationBubbleSyncController(BrowserWindowInterface* bwi,
                                           Profile* profile);
  ~ProfileCustomizationBubbleSyncController();

  ProfileCustomizationBubbleSyncController(
      const ProfileCustomizationBubbleSyncController& other) = delete;
  ProfileCustomizationBubbleSyncController& operator=(
      const ProfileCustomizationBubbleSyncController& other) = delete;

  // Applies `suggested_profile_color` and shows the profile customization
  // bubble if either (a) theme sync cannot start (see `CanThemeSyncStart()`) or
  // (b) theme sync is successful and results in the default theme in this
  // profile (either no value was synced before or the default theme was
  // synced). In all other cases, the call has no visible impact. This also
  // includes the case when sync can start but is blocked on the user to enter
  // the custom passphrase.
  void ShowOnSyncFailedOrDefaultTheme(SkColor suggested_profile_color);
  void ShowOnSyncFailedOrDefaultThemeForTesting(
      SkColor suggested_profile_color,
      ShowBubbleCallback show_bubble_callback_testing_override,
      syncer::SyncService* sync_service_testing_override,
      ThemeService* theme_service_testing_override,
      NtpCustomBackgroundService*
          ntp_custom_background_service_testing_override);

  bool IsWaitingForTheme() const;

  // Returns whether theme sync can start (i.e. is not disabled by policy,
  // theme sync is enabled, ...).
  static bool CanThemeSyncStart(Profile* profile);

 private:
  // Note: Both `sync_service` and `theme_service` must outlive `this`.
  void ShowOnSyncFailedOrDefaultThemeInternal(
      SkColor suggested_profile_color,
      ShowBubbleCallback show_bubble_callback,
      syncer::SyncService* sync_service,
      ThemeService* theme_service,
      NtpCustomBackgroundService* ntp_custom_background_service);
  void OnSyncedThemeReady(
      SkColor suggested_profile_color,
      ThemeService* theme_service,
      NtpCustomBackgroundService* ntp_custom_background_service,
      ProfileCustomizationSyncedThemeWaiter::Outcome outcome);

  // Functions that finalize the control logic by either showing or skipping the
  // bubble (or aborting completely).
  void ApplyDefaultColorAndShowBubble(SkColor suggested_profile_color,
                                      ThemeService* theme_service);
  void MaybeInvokeCallback(Outcome outcome);

  const raw_ref<BrowserWindowInterface> bwi_;
  const raw_ref<Profile> profile_;
  std::unique_ptr<ProfileCustomizationSyncedThemeWaiter> theme_waiter_;
  ShowBubbleCallback show_bubble_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_SYNC_CONTROLLER_H_
