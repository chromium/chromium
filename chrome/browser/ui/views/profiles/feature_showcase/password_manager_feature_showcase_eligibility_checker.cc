// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/password_manager_feature_showcase_eligibility_checker.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_constants.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

PasswordManagerFeatureShowcaseEligibilityChecker::
    PasswordManagerFeatureShowcaseEligibilityChecker() = default;

PasswordManagerFeatureShowcaseEligibilityChecker::
    ~PasswordManagerFeatureShowcaseEligibilityChecker() = default;

void PasswordManagerFeatureShowcaseEligibilityChecker::CheckEligibility(
    Profile& profile,
    base::OnceCallback<void(bool)> callback) {
  if (!profile.GetPrefs()->GetBoolean(
          password_manager::prefs::kCredentialsEnableService)) {
    std::move(callback).Run(false);
    return;
  }

  profile_ = &profile;
  callback_ = std::move(callback);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (!sync_service ||
      sync_service->GetActiveDataTypes().Has(syncer::PREFERENCES) ||
      sync_service->GetTransportState() ==
          syncer::SyncService::TransportState::DISABLED ||
      sync_service->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED ||
      !sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPreferences)) {
    // If sync is unavailable, evaluate immediately based on the current state.
    RunCallbackAndStopObserving();
    return;
  }

  sync_service_observation_.Observe(sync_service);
}

std::string
PasswordManagerFeatureShowcaseEligibilityChecker::GetStepIdentifier() const {
  return kFeatureShowcasePasswordManagerStepIdentifier;
}

bool PasswordManagerFeatureShowcaseEligibilityChecker::OnTimeout() {
  sync_service_observation_.Reset();
  callback_.Reset();

  if (!profile_) {
    return false;
  }

  PinnedToolbarActionsModel& pinned_actions_model =
      CHECK_DEREF(PinnedToolbarActionsModel::Get(profile_));
  return !pinned_actions_model.Contains(kActionShowPasswordsBubbleOrPage);
}

void PasswordManagerFeatureShowcaseEligibilityChecker::OnStateChanged(
    syncer::SyncService* sync) {
  if (sync->GetActiveDataTypes().Has(syncer::PREFERENCES) ||
      sync->GetTransportState() ==
          syncer::SyncService::TransportState::DISABLED ||
      sync->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED ||
      !sync->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kPreferences)) {
    RunCallbackAndStopObserving();
  }
}

void PasswordManagerFeatureShowcaseEligibilityChecker::OnSyncShutdown(
    syncer::SyncService* sync) {
  RunCallbackAndStopObserving();
}

void PasswordManagerFeatureShowcaseEligibilityChecker::
    RunCallbackAndStopObserving() {
  sync_service_observation_.Reset();

  if (callback_) {
    PinnedToolbarActionsModel& pinned_actions_model =
        CHECK_DEREF(PinnedToolbarActionsModel::Get(profile_));
    std::move(callback_).Run(
        !pinned_actions_model.Contains(kActionShowPasswordsBubbleOrPage));
  }
}
