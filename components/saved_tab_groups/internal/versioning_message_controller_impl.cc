// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/versioning_message_controller_impl.h"

#include "base/functional/bind.h"
#include "components/data_sharing/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/utils.h"

namespace tab_groups {
namespace {

// Represents various possible version states based on feature flags.
enum class VersionState {
  // Version is out of date. Versioning messages to update chrome can be shown.
  // Feature flags:
  // data_sharing::features::kSharedDataTypesKillSwitch ENABLED,
  // data_sharing::features::kDataSharingEnableUpdateChromeUI ENABLED.
  kOutOfDate,

  // Version is out of date. However, no specific versioning message should be
  // shown.
  // Feature flags:
  // data_sharing::features::kSharedDataTypesKillSwitch ENABLED,
  // data_sharing::features::kDataSharingEnableUpdateChromeUI DISABLED.
  kNoMessage,

  // Version is up-to-date.
  // Feature flags:
  // data_sharing::features::kSharedDataTypesKillSwitch DISABLED,
  // data_sharing::features::kDataSharingEnableUpdateChromeUI DISABLED.
  kUpToDate,

  // Invalid combination of feature flags. No specific versioning message should
  // be shown.
  // Feature flags:
  // data_sharing::features::kSharedDataTypesKillSwitch DISABLED,
  // data_sharing::features::kDataSharingEnableUpdateChromeUI ENABLED.
  kInvalidCombination,

  // Data sharing main feature flags are disabled. No versioning message should
  // be shown and we shouldn't even bother to set or reset pref state. This
  // state can be cleaned up after data sharing feature launch.
  // Feature flags:
  // data_sharing::features::IsDataSharingFunctionalityEnabled() => false
  // data_sharing::features::kSharedDataTypesKillSwitch ANY STATE,
  // data_sharing::features::kDataSharingEnableUpdateChromeUI ANY STATE.
  kDataSharingFunctionalityDisabled,
};

// Returns the current version state based on combination of feature flags.
VersionState GetVersionState() {
  if (!data_sharing::features::IsDataSharingFunctionalityEnabled()) {
    return VersionState::kDataSharingFunctionalityDisabled;
  }
  const bool is_shared_data_types_enabled = !base::FeatureList::IsEnabled(
      data_sharing::features::kSharedDataTypesKillSwitch);
  const bool is_update_chrome_ui_enabled = base::FeatureList::IsEnabled(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);

  if (is_shared_data_types_enabled) {
    return is_update_chrome_ui_enabled ? VersionState::kInvalidCombination
                                       : VersionState::kUpToDate;
  } else {
    return is_update_chrome_ui_enabled ? VersionState::kOutOfDate
                                       : VersionState::kNoMessage;
  }
}

bool HasCurrentSharedTabGroups(TabGroupSyncService* tab_group_sync_service) {
  for (const auto* saved_tab_group : tab_group_sync_service->ReadAllGroups()) {
    if (saved_tab_group->collaboration_id().has_value()) {
      return true;
    }
  }

  return false;
}

}  // namespace

VersioningMessageControllerImpl::VersioningMessageControllerImpl(
    PrefService* pref_service,
    TabGroupSyncService* tab_group_sync_service)
    : pref_service_(pref_service),
      tab_group_sync_service_(tab_group_sync_service) {
  tab_group_sync_service_->AddObserver(this);
}

VersioningMessageControllerImpl::~VersioningMessageControllerImpl() {
  tab_group_sync_service_->RemoveObserver(this);
}

void VersioningMessageControllerImpl::ComputePrefsOnStartup() {
  // On startup, read and compute the state of the prefs based on the feature
  // flag and number of shared tab groups in the previous session.

  switch (GetVersionState()) {
    case VersionState::kOutOfDate: {
      // Version is out-of-date. If there were shared tab groups last session,
      // that means that the version just switched. Compute the pref states
      // accordingly.

      // Determine if previous session had shared tab groups that make
      // it eligible for the instant/persistent message.
      const bool had_open_shared_tab_groups =
          tab_group_sync_service_->HadSharedTabGroupsLastSession(
              /*open_shared_tab_groups=*/true);
      const bool had_any_shared_tab_groups =
          tab_group_sync_service_->HadSharedTabGroupsLastSession(
              /*open_shared_tab_groups=*/false);

      if (had_open_shared_tab_groups) {
        pref_service_->SetBoolean(
            prefs::kEligibleForVersionOutOfDateInstantMessage, true);
      }

      if (had_any_shared_tab_groups) {
        pref_service_->SetBoolean(
            prefs::kEligibleForVersionOutOfDatePersistentMessage, true);

        // For desktop, instant message should be shown if there are any shared
        // tab groups in the previous session regardless of whether they were
        // open or closed.
        if (!AreLocalIdsPersisted()) {
          pref_service_->SetBoolean(
              prefs::kEligibleForVersionOutOfDateInstantMessage, true);
        }
      }

      // Always reset the 'updated' message eligibility when out of date.
      pref_service_->SetBoolean(prefs::kEligibleForVersionUpdatedMessage,
                                false);
      break;
    }
    case VersionState::kUpToDate: {
      // Version is up-to-date. Determine if eligible for the 'version updated'
      // message.
      const bool had_any_out_of_date_messages_before =
          pref_service_->GetBoolean(prefs::kHasShownAnyVersionOutOfDateMessage);

      if (had_any_out_of_date_messages_before) {
        pref_service_->SetBoolean(prefs::kEligibleForVersionUpdatedMessage,
                                  true);
      }

      // Always reset the 'out-of-date' message eligibilities when up to date.
      pref_service_->SetBoolean(
          prefs::kEligibleForVersionOutOfDateInstantMessage, false);
      pref_service_->SetBoolean(prefs::kHasShownAnyVersionOutOfDateMessage,
                                false);
      pref_service_->SetBoolean(
          prefs::kEligibleForVersionOutOfDatePersistentMessage, false);
      break;
    }
    case VersionState::kNoMessage:
    case VersionState::kInvalidCombination:
    case VersionState::kDataSharingFunctionalityDisabled: {
      // In these states, no specific versioning messages are tied to the
      // feature flag combination. The prefs should carry over their previous
      // state or default values if not explicitly set elsewhere.
      break;
    }
  }
}

bool VersioningMessageControllerImpl::ShouldShowMessageUi(
    MessageType message_type) {
  CHECK(is_initialized_);

  VersionState current_version_state = GetVersionState();
  switch (message_type) {
    case MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE:
      return current_version_state == VersionState::kOutOfDate &&
             pref_service_->GetBoolean(
                 prefs::kEligibleForVersionOutOfDateInstantMessage);
    case MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE:
      return current_version_state == VersionState::kOutOfDate &&
             pref_service_->GetBoolean(
                 prefs::kEligibleForVersionOutOfDatePersistentMessage);
    case MessageType::VERSION_UPDATED_MESSAGE:
      return current_version_state == VersionState::kUpToDate &&
             pref_service_->GetBoolean(
                 prefs::kEligibleForVersionUpdatedMessage) &&
             HasCurrentSharedTabGroups(tab_group_sync_service_);
    default:
      return false;
  }
}

bool VersioningMessageControllerImpl::IsInitialized() {
  return is_initialized_;
}

void VersioningMessageControllerImpl::ShouldShowMessageUiAsync(
    MessageType message_type,
    base::OnceCallback<void(bool)> callback) {
  // The VERSION_UPDATED_MESSAGE requires waiting for tab groups to be
  // downloaded. We handle it separately by queueing the callback.
  if (message_type == MessageType::VERSION_UPDATED_MESSAGE) {
    // If the check has already completed and final state of the eligibility is
    // already determined, we can respond immediately with the final state.
    if (processed_version_updated_callbacks_) {
      std::move(callback).Run(ShouldShowMessageUi(message_type));
    } else {
      // Otherwise, queue the callback. It will be run later by either
      // OnInitialized or OnTabGroupAdded.
      pending_version_updated_callbacks_.push_back(std::move(callback));
    }
    return;
  }

  // For all other message types, queue callbacks if init isn't complete,
  // otherwise invoke them right away.
  if (!is_initialized_) {
    pending_callbacks_.push_back(base::BindOnce(
        &VersioningMessageControllerImpl::ShouldShowMessageUiAsync,
        weak_ptr_factory_.GetWeakPtr(), message_type, std::move(callback)));

    return;
  }

  std::move(callback).Run(ShouldShowMessageUi(message_type));
}

void VersioningMessageControllerImpl::OnMessageUiShown(
    MessageType message_type) {
  switch (message_type) {
    case MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE:
      pref_service_->SetBoolean(
          prefs::kEligibleForVersionOutOfDateInstantMessage, false);
      pref_service_->SetBoolean(prefs::kHasShownAnyVersionOutOfDateMessage,
                                true);
      break;
    case MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE:
      pref_service_->SetBoolean(prefs::kHasShownAnyVersionOutOfDateMessage,
                                true);
      break;
    case MessageType::VERSION_UPDATED_MESSAGE:
      pref_service_->SetBoolean(prefs::kEligibleForVersionUpdatedMessage,
                                false);
      break;
    default:
      break;
  }
}

void VersioningMessageControllerImpl::OnMessageUiDismissed(
    MessageType message_type) {
  if (message_type == MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE) {
    pref_service_->SetBoolean(
        prefs::kEligibleForVersionOutOfDatePersistentMessage, false);
  }
}

void VersioningMessageControllerImpl::OnInitialized() {
  is_initialized_ = true;
  ComputePrefsOnStartup();

  // Attempt to resolve any pending VERSION_UPDATED_MESSAGE callbacks. This
  // handles cases where conditions are already met (or definitively not met)
  // at startup.
  MaybeResolvePendingVersionUpdatedCallbacks();

  // Run pending callbacks for other message types that were waiting for init.
  for (auto& callback : pending_callbacks_) {
    std::move(callback).Run();
  }
  pending_callbacks_.clear();
}

void VersioningMessageControllerImpl::OnTabGroupAdded(
    const SavedTabGroup& group,
    TriggerSource source) {
  // We only care about the first shared group that appears.
  if (processed_version_updated_callbacks_ || !group.is_shared_tab_group()) {
    return;
  }

  // A shared group has been added. Re-evaluate the conditions to see if we
  // can resolve the pending callbacks now.
  MaybeResolvePendingVersionUpdatedCallbacks();
}

void VersioningMessageControllerImpl::
    MaybeResolvePendingVersionUpdatedCallbacks() {
  if (processed_version_updated_callbacks_) {
    return;
  }

  // The message can only be shown if the version is up to date and the user is
  // eligible (they have seen an out-of-date message before).
  const bool can_ever_show =
      GetVersionState() == VersionState::kUpToDate &&
      pref_service_->GetBoolean(prefs::kEligibleForVersionUpdatedMessage);

  bool should_resolve = false;
  bool resolution_value = false;

  if (!can_ever_show) {
    // If base conditions aren't met, we can definitively resolve with `false`.
    should_resolve = true;
    resolution_value = false;
  } else if (HasCurrentSharedTabGroups(tab_group_sync_service_)) {
    // If base conditions *are* met and we have shared groups, resolve with
    // `true`.
    should_resolve = true;
    resolution_value = true;
  }

  // If `should_resolve` is false, it means conditions are plausible but we are
  // still waiting for a shared tab group to be added. We'll wait for another
  // call from OnTabGroupAdded.
  if (!should_resolve) {
    return;
  }

  for (auto& callback : pending_version_updated_callbacks_) {
    std::move(callback).Run(resolution_value);
  }
  pending_version_updated_callbacks_.clear();
  processed_version_updated_callbacks_ = true;
}

}  // namespace tab_groups
