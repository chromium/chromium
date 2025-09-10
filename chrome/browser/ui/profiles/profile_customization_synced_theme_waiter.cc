// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/profile_customization_synced_theme_waiter.h"

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/themes/theme_service.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

ProfileCustomizationSyncedThemeWaiter::ProfileCustomizationSyncedThemeWaiter(
    syncer::SyncService* sync_service,
    ThemeService* theme_service,
    base::OnceCallback<void(Outcome)> callback)
    : sync_service_(sync_service),
      theme_service_(theme_service),
      callback_(std::move(callback)) {
  DCHECK(sync_service_);
  DCHECK(theme_service_);
  DCHECK(callback_);
}

ProfileCustomizationSyncedThemeWaiter::
    ~ProfileCustomizationSyncedThemeWaiter() = default;

// static
bool ProfileCustomizationSyncedThemeWaiter::CanThemeSyncStart(
    syncer::SyncService* sync_service) {
  return sync_service && sync_service->CanSyncFeatureStart() &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kThemes);
}

void ProfileCustomizationSyncedThemeWaiter::Run() {
#if DCHECK_IS_ON()
  DCHECK(!is_running_);
  is_running_ = true;
#endif  // DCHECK_IS_ON()

  if (!CheckThemeSyncPreconditions()) {
    return;
  }

  std::optional<ThemeSyncableService::ThemeSyncState> theme_state =
      theme_service_->GetThemeSyncableService()->GetThemeSyncStartState();
  if (theme_state) {
    OnThemeSyncStarted(*theme_state);
    return;
  }

  constexpr base::TimeDelta kSyncedThemeTimeout = base::Seconds(3);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ProfileCustomizationSyncedThemeWaiter::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      kSyncedThemeTimeout);

  sync_observation_.Observe(sync_service_);
  theme_sync_observation_.Observe(theme_service_->GetThemeSyncableService());
}

void ProfileCustomizationSyncedThemeWaiter::OnStateChanged(
    syncer::SyncService* sync) {
  CheckThemeSyncPreconditions();
}

void ProfileCustomizationSyncedThemeWaiter::OnThemeSyncStarted(
    ThemeSyncableService::ThemeSyncState state) {
  switch (state) {
    case ThemeSyncableService::ThemeSyncState::kWaitingForExtensionInstallation:
      // Wait for the extension theme to be installed to avoid flicker in the
      // profile customization dialog.
      theme_observation_.Observe(theme_service_);
      return;
    case ThemeSyncableService::ThemeSyncState::kFailed:
    case ThemeSyncableService::ThemeSyncState::kApplied:
      InvokeCallback(Outcome::kSyncSuccess);
      return;
  }
}

void ProfileCustomizationSyncedThemeWaiter::OnThemeChanged() {
  InvokeCallback(Outcome::kSyncSuccess);
}

void ProfileCustomizationSyncedThemeWaiter::OnTimeout() {
  if (!callback_) {
    return;
  }

  InvokeCallback(Outcome::kTimeout);
}

void ProfileCustomizationSyncedThemeWaiter::InvokeCallback(Outcome outcome) {
  sync_observation_.Reset();
  theme_sync_observation_.Reset();
  theme_observation_.Reset();

  std::move(callback_).Run(outcome);
  // `this` may be deleted.
}

bool ProfileCustomizationSyncedThemeWaiter::CheckThemeSyncPreconditions() {
  if (!CanThemeSyncStart(sync_service_)) {
    InvokeCallback(Outcome::kSyncCannotStart);
    return false;
  }

  if (sync_service_->GetUserSettings()->IsPassphraseRequired()) {
    InvokeCallback(Outcome::kSyncPassphraseRequired);
    return false;
  }

  return true;
}
