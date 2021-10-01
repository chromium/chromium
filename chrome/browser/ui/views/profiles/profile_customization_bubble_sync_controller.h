// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_SYNC_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_SYNC_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class View;
}  // namespace views

class Profile;

// Helper class for logic to show / delay showing the profile customization
// bubble. Owns itself.
class ProfileCustomizationBubbleSyncController
    : public syncer::SyncServiceObserver,
      public ThemeSyncableService::Observer,
      public ProfileObserver,
      public views::ViewObserver {
 public:
  enum class Outcome {
    kShowBubble,
    kSkipBubble,
    // The browser is being destroyed.
    kAbort
  };
  using ShowBubbleCallback = base::OnceCallback<void(Outcome outcome)>;

  ~ProfileCustomizationBubbleSyncController() override;

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
  static void ApplyColorAndShowBubbleWhenNoValueSynced(
      Profile* profile,
      views::View* anchor_view,
      SkColor suggested_profile_color);

  // A version of ApplyColorAndShowBubbleWhenNoValueSynced() that allows simpler
  // mocking.
  static void ApplyColorAndShowBubbleWhenNoValueSyncedForTesting(
      Profile* profile,
      views::View* anchor_view,
      syncer::SyncService* sync_service,
      ThemeService* theme_service,
      ShowBubbleCallback show_bubble_callback,
      SkColor suggested_profile_color);

  // Returns whether theme sync can start (i.e. is not disabled by policy,
  // theme sync is enabled, ...).
  static bool CanThemeSyncStart(Profile* profile);

 private:
  ProfileCustomizationBubbleSyncController(
      Profile* profile,
      views::View* anchor_view,
      syncer::SyncService* sync_service,
      ThemeService* theme_service,
      ShowBubbleCallback show_bubble_callback,
      SkColor suggested_profile_color);

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // ThemeSyncableService::Observer:
  void OnThemeSyncStarted(ThemeSyncableService::ThemeSyncState state) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

  // This function may delete the object.
  void Init();

  // Functions that finalize the control logic by either showing or skipping the
  // bubble (or aborting completely) and deleting itself.
  void ApplyDefaultColorAndShowBubble();
  void SkipBubble();
  void Abort();

  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<ThemeService> theme_service_;
  ShowBubbleCallback show_bubble_callback_;
  SkColor const suggested_profile_color_;
  base::TimeTicks observation_start_time_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  base::ScopedObservation<ThemeSyncableService, ThemeSyncableService::Observer>
      theme_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_SYNC_CONTROLLER_H_
