// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/versioning_message_controller_impl.h"

#include "base/functional/bind.h"
#include "components/data_sharing/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {
namespace {
bool IsVersionOutOfDate() {
  return base::FeatureList::IsEnabled(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
}

bool MeetsEligibilityCriteria(
    PrefService* pref_service,
    VersioningMessageController::MessageType message_type) {
  switch (message_type) {
    case VersioningMessageController::MessageType::
        VERSION_OUT_OF_DATE_INSTANT_MESSAGE:
      return !pref_service->GetBoolean(
          prefs::kDataSharingHasShownVersionOutOfDateInstantMessage);
    case VersioningMessageController::MessageType::
        VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE:
      return !pref_service->GetBoolean(
          prefs::kDataSharingHasDismissedVersionOutOfDatePersistentMessage);
    case VersioningMessageController::MessageType::VERSION_UPDATED_MESSAGE:
      return !pref_service->GetBoolean(
          prefs::kDataSharingHasShownVersionUpdatedMessage);
    default:
      return false;
  }
}

}  // namespace

VersioningMessageControllerImpl::VersioningMessageControllerImpl(
    PrefService* pref_service,
    TabGroupSyncService* tab_group_sync_service)
    : pref_service_(pref_service),
      tab_group_sync_service_(tab_group_sync_service) {
  ResetMessagePrefsOnStartup();
  tab_group_sync_service_->AddObserver(this);
}

VersioningMessageControllerImpl::~VersioningMessageControllerImpl() {
  tab_group_sync_service_->RemoveObserver(this);
}

void VersioningMessageControllerImpl::ResetMessagePrefsOnStartup() {
  // On startup, reset the message prefs based on the state of the
  // feature flag.
  if (IsVersionOutOfDate()) {
    // If versioning is disabled, reset the pref used for showing the re-enabled
    // message shown in the enabled state.
    pref_service_->SetBoolean(prefs::kDataSharingHasShownVersionUpdatedMessage,
                              false);
  } else {
    // If versioning is enabled, reset the prefs used for showing the instant
    // and persistent messages shown in the disabled state.
    pref_service_->SetBoolean(
        prefs::kDataSharingHasShownVersionOutOfDateInstantMessage, false);
    pref_service_->SetBoolean(
        prefs::kDataSharingHasDismissedVersionOutOfDatePersistentMessage,
        false);
  }
}

void VersioningMessageControllerImpl::ShouldShowMessageUiAsync(
    MessageType message_type,
    base::OnceCallback<void(bool)> callback) {
  if (!is_initialized_) {
    pending_callbacks_.push_back(base::BindOnce(
        &VersioningMessageControllerImpl::ShouldShowMessageUiAsync,
        weak_ptr_factory_.GetWeakPtr(), message_type, std::move(callback)));

    return;
  }

  std::move(callback).Run(ShouldShowMessageUi(message_type));
}

bool VersioningMessageControllerImpl::ShouldShowMessageUi(
    MessageType message_type) {
  CHECK(is_initialized_);
  bool is_version_out_of_date = IsVersionOutOfDate();
  bool had_open_shared_tab_groups =
      tab_group_sync_service_->HadSharedTabGroupsLastSession(
          /*open_shared_tab_groups=*/true);
  bool had_shared_tab_groups =
      tab_group_sync_service_->HadSharedTabGroupsLastSession(
          /*open_shared_tab_groups=*/false);
  // Determines if the message can be shown, considering past displays or
  // dismissals.
  bool meets_eligibility_criteria =
      MeetsEligibilityCriteria(pref_service_, message_type);

  switch (message_type) {
    case MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE:
      return is_version_out_of_date && had_open_shared_tab_groups &&
             meets_eligibility_criteria;
    case MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE:
      return is_version_out_of_date && had_shared_tab_groups &&
             meets_eligibility_criteria;
    case MessageType::VERSION_UPDATED_MESSAGE:
      return !is_version_out_of_date && meets_eligibility_criteria;
    default:
      return false;
  }
}

void VersioningMessageControllerImpl::OnMessageUiShown(
    MessageType message_type) {
  switch (message_type) {
    case MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE:
      pref_service_->SetBoolean(
          prefs::kDataSharingHasShownVersionOutOfDateInstantMessage, true);
      break;
    case MessageType::VERSION_UPDATED_MESSAGE:
      pref_service_->SetBoolean(
          prefs::kDataSharingHasShownVersionUpdatedMessage, true);
      break;
    default:
      break;
  }
}

void VersioningMessageControllerImpl::OnMessageUiDismissed(
    MessageType message_type) {
  if (message_type == MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE) {
    pref_service_->SetBoolean(
        prefs::kDataSharingHasDismissedVersionOutOfDatePersistentMessage, true);
  }
}

void VersioningMessageControllerImpl::OnInitialized() {
  is_initialized_ = true;
  for (auto& callback : pending_callbacks_) {
    std::move(callback).Run();
  }
  pending_callbacks_.clear();
}

}  // namespace tab_groups
