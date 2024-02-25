// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"

#include <set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/multidevice_setup/host_device_timestamp_manager.h"
#include "chromeos/ash/services/multidevice_setup/host_status_provider_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {

namespace multidevice_setup {

namespace {

const int64_t kTimestampNotSet = 0;
const char kNoHost[] = "";

}  // namespace

// static
AccountStatusChangeDelegateNotifierImpl::Factory*
    AccountStatusChangeDelegateNotifierImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<AccountStatusChangeDelegateNotifier>
AccountStatusChangeDelegateNotifierImpl::Factory::Create(
    HostStatusProvider* host_status_provider,
    PrefService* pref_service,
    HostDeviceTimestampManager* host_device_timestamp_manager,
    OobeCompletionTracker* oobe_completion_tracker,
    base::Clock* clock) {
  if (test_factory_) {
    return test_factory_->CreateInstance(host_status_provider, pref_service,
                                         host_device_timestamp_manager,
                                         oobe_completion_tracker, clock);
  }
  return base::WrapUnique(new AccountStatusChangeDelegateNotifierImpl(
      host_status_provider, pref_service, host_device_timestamp_manager,
      oobe_completion_tracker, clock));
}

// static
void AccountStatusChangeDelegateNotifierImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

AccountStatusChangeDelegateNotifierImpl::Factory::~Factory() = default;

// static
void AccountStatusChangeDelegateNotifierImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  // Records the timestamps (in milliseconds since UNIX Epoch, aka JavaTime) of
  // the last instance the delegate was notified for each of the changes listed
  // in the class description.
  registry->RegisterInt64Pref(kNewUserPotentialHostExistsPrefName,
                              kTimestampNotSet);
  registry->RegisterInt64Pref(kExistingUserHostSwitchedPrefName,
                              kTimestampNotSet);
  registry->RegisterInt64Pref(kExistingUserChromebookAddedPrefName,
                              kTimestampNotSet);

  registry->RegisterInt64Pref(kOobeSetupFlowTimestampPrefName,
                              kTimestampNotSet);
  registry->RegisterStringPref(
      kVerifiedHostDeviceIdFromMostRecentHostStatusUpdatePrefName, kNoHost);

  registry->RegisterInt64Pref(kMultiDeviceLastSessionStartTime,
                              kTimestampNotSet);
}

AccountStatusChangeDelegateNotifierImpl::
    ~AccountStatusChangeDelegateNotifierImpl() {
  host_status_provider_->RemoveObserver(this);
  oobe_completion_tracker_->RemoveObserver(this);
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void AccountStatusChangeDelegateNotifierImpl::OnDelegateSet() {
  CheckForMultiDeviceEvents(host_status_provider_->GetHostWithStatus());
}

// static
const char AccountStatusChangeDelegateNotifierImpl::
    kNewUserPotentialHostExistsPrefName[] =
        "multidevice_setup.new_user_potential_host_exists";

// static
const char AccountStatusChangeDelegateNotifierImpl::
    kExistingUserHostSwitchedPrefName[] =
        "multidevice_setup.existing_user_host_switched";

// static
const char AccountStatusChangeDelegateNotifierImpl::
    kExistingUserChromebookAddedPrefName[] =
        "multidevice_setup.existing_user_chromebook_added";

// Note that, despite the pref string name, this pref only records the IDs of
// verified hosts. In particular, if a host has been set but is waiting for
// verification, it will not recorded.
// static
const char AccountStatusChangeDelegateNotifierImpl::
    kVerifiedHostDeviceIdFromMostRecentHostStatusUpdatePrefName[] =
        "multidevice_setup.host_device_id_from_most_recent_sync";

// The timestamps (in milliseconds since UNIX Epoch, aka JavaTime) of the user
// seeing setup flow in OOBE. If it is 0, the user did not see the setup flow in
// OOBE.
// static
const char
    AccountStatusChangeDelegateNotifierImpl::kOobeSetupFlowTimestampPrefName[] =
        "multidevice_setup.oobe_setup_flow_timestamp ";

// Used to verify if multi device setup notification should be shown.
const char AccountStatusChangeDelegateNotifierImpl::
    kMultiDeviceLastSessionStartTime[] =
        "multidevice_setup.last_session_start_time";

AccountStatusChangeDelegateNotifierImpl::
    AccountStatusChangeDelegateNotifierImpl(
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        HostDeviceTimestampManager* host_device_timestamp_manager,
        OobeCompletionTracker* oobe_completion_tracker,
        base::Clock* clock)
    : host_status_provider_(host_status_provider),
      pref_service_(pref_service),
      host_device_timestamp_manager_(host_device_timestamp_manager),
      oobe_completion_tracker_(oobe_completion_tracker),
      clock_(clock) {
  verified_host_device_id_from_most_recent_update_ =
      LoadHostDeviceIdFromEndOfPreviousSession();
  host_status_provider_->AddObserver(this);
  oobe_completion_tracker_->AddObserver(this);
  session_manager::SessionManager::Get()->AddObserver(this);
  if (IsInPhoneHubNotificationExperimentGroup()) {
    // In the object is created after OnSessionStateChanged() is already called,
    // manually update the timestamp.
    UpdateSessionStartTimeIfEligible();
  }
}

void AccountStatusChangeDelegateNotifierImpl::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  CheckForMultiDeviceEvents(host_status_with_device);
}

void AccountStatusChangeDelegateNotifierImpl::OnOobeCompleted() {
  pref_service_->SetInt64(kOobeSetupFlowTimestampPrefName,
                          clock_->Now().InMillisecondsSinceUnixEpoch());
  if (delegate())
    delegate()->OnNoLongerNewUser();
}

void AccountStatusChangeDelegateNotifierImpl::OnSessionStateChanged() {
  UpdateSessionStartTimeIfEligible();
}

void AccountStatusChangeDelegateNotifierImpl::
    UpdateSessionStartTimeIfEligible() {
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked()) {
    return;
  }
  if (IsInPhoneHubNotificationExperimentGroup()) {
    pref_service_->SetInt64(kMultiDeviceLastSessionStartTime,
                            clock_->Now().InMillisecondsSinceUnixEpoch());
    CheckForNewUserPotentialHostExistsEvent(
        host_status_provider_->GetHostWithStatus());
  }
}

bool AccountStatusChangeDelegateNotifierImpl::
    IsInPhoneHubNotificationExperimentGroup() {
  return features::IsPhoneHubOnboardingNotifierRevampEnabled() &&
         !features::kPhoneHubOnboardingNotifierUseNudge.Get();
}

void AccountStatusChangeDelegateNotifierImpl::CheckForMultiDeviceEvents(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  if (!delegate()) {
    PA_LOG(WARNING)
        << "AccountStatusChangeDelegateNotifierImpl::"
        << "CheckForMultiDeviceEvents(): Tried to check for potential "
        << "events, but no delegate was set.";
    return;
  }

  // Track and update host status.
  std::optional<mojom::HostStatus> host_status_before_update =
      host_status_from_most_recent_update_;
  host_status_from_most_recent_update_ = host_status_with_device.host_status();

  // Track and update verified host info.
  std::optional<std::string> verified_host_device_id_before_update =
      verified_host_device_id_from_most_recent_update_;

  // Check if a host has been verified.
  if (host_status_with_device.host_status() ==
      mojom::HostStatus::kHostVerified) {
    verified_host_device_id_from_most_recent_update_ =
        host_status_with_device.host_device()->GetDeviceId();
    pref_service_->SetString(
        kVerifiedHostDeviceIdFromMostRecentHostStatusUpdatePrefName,
        *verified_host_device_id_from_most_recent_update_);
  } else {
    // No host set.
    verified_host_device_id_from_most_recent_update_.reset();
    pref_service_->SetString(
        kVerifiedHostDeviceIdFromMostRecentHostStatusUpdatePrefName, kNoHost);
  }

  CheckForNewUserPotentialHostExistsEvent(host_status_with_device);
  CheckForNoLongerNewUserEvent(host_status_with_device,
                               host_status_before_update);
  CheckForExistingUserHostSwitchedEvent(host_status_with_device,
                                        verified_host_device_id_before_update);
  CheckForExistingUserChromebookAddedEvent(
      host_status_with_device, verified_host_device_id_before_update);
}

void AccountStatusChangeDelegateNotifierImpl::
    CheckForNewUserPotentialHostExistsEvent(
        const HostStatusProvider::HostStatusWithDevice&
            host_status_with_device) {
  if (!features::IsPhoneHubOnboardingNotifierRevampEnabled()) {
    // We do not notify the user if they already had a chance to go through
    // setup flow in OOBE.
    if (pref_service_->GetInt64(kOobeSetupFlowTimestampPrefName) !=
        kTimestampNotSet) {
      return;
    }
  } else {
    if (!IsInPhoneHubNotificationExperimentGroup()) {
      // The user is in group for nudge. Do not show notification.
      return;
    }
  }

  // We only check for new user events if there is no enabled host.
  if (verified_host_device_id_from_most_recent_update_)
    return;

  // If the observer has been notified of a potential verified host in the past,
  // they are not considered a new user.
  if (pref_service_->GetInt64(kNewUserPotentialHostExistsPrefName) !=
          kTimestampNotSet ||
      pref_service_->GetInt64(kExistingUserChromebookAddedPrefName) !=
          kTimestampNotSet) {
    return;
  }

  // kEligibleHostExistsButNoHostSet is the only HostStatus that can describe
  // a new user.
  if (host_status_with_device.host_status() !=
      mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
    return;
  }

  if (IsInPhoneHubNotificationExperimentGroup()) {
    if (pref_service_->GetInt64(kMultiDeviceLastSessionStartTime) !=
            kTimestampNotSet &&
        clock_->Now() -
                base::Time::FromMillisecondsSinceUnixEpoch(
                    pref_service_->GetInt64(kMultiDeviceLastSessionStartTime)) >
            features::kMultiDeviceSetupNotificationTimeLimit.Get()) {
      return;
    }
  }

  if (delegate()) {
    delegate()->OnPotentialHostExistsForNewUser();
    pref_service_->SetInt64(kNewUserPotentialHostExistsPrefName,
                            clock_->Now().InMillisecondsSinceUnixEpoch());
  }
}

void AccountStatusChangeDelegateNotifierImpl::CheckForNoLongerNewUserEvent(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device,
    const std::optional<mojom::HostStatus> host_status_before_update) {
  // We are only looking for the case when the host status switched from
  // kEligibleHostExistsButNoHostSet to something else.
  if (host_status_with_device.host_status() ==
          mojom::HostStatus::kEligibleHostExistsButNoHostSet ||
      host_status_before_update !=
          mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
    return;
  }

  // If the user has ever had a verified host, they have already left the 'new
  // user' state.
  if (pref_service_->GetInt64(kExistingUserChromebookAddedPrefName) !=
      kTimestampNotSet) {
    return;
  }

  delegate()->OnNoLongerNewUser();
}

void AccountStatusChangeDelegateNotifierImpl::
    CheckForExistingUserHostSwitchedEvent(
        const HostStatusProvider::HostStatusWithDevice& host_status_with_device,
        const std::optional<std::string>&
            verified_host_device_id_before_update) {
  // The host switched event requires both a pre-update and a post-update
  // verified host.
  if (!verified_host_device_id_from_most_recent_update_ ||
      !verified_host_device_id_before_update) {
    return;
  }

  // If the host stayed the same, there was no switch.
  if (*verified_host_device_id_from_most_recent_update_ ==
      *verified_host_device_id_before_update) {
    return;
  }

  delegate()->OnConnectedHostSwitchedForExistingUser(
      host_status_with_device.host_device()->name());
  pref_service_->SetInt64(kExistingUserHostSwitchedPrefName,
                          clock_->Now().InMillisecondsSinceUnixEpoch());
}

void AccountStatusChangeDelegateNotifierImpl::
    CheckForExistingUserChromebookAddedEvent(
        const HostStatusProvider::HostStatusWithDevice& host_status_with_device,
        const std::optional<std::string>&
            verified_host_device_id_before_update) {
  // The Chromebook added event requires that a verified host was found by the
  // update, i.e. there was no verified host before the host status update but
  // afterward there was a verified host.
  if (!verified_host_device_id_from_most_recent_update_ ||
      verified_host_device_id_before_update) {
    return;
  }

  // This event is specific to setup taking place on a different Chromebook.
  if (host_device_timestamp_manager_->WasHostSetFromThisChromebook())
    return;

  delegate()->OnNewChromebookAddedForExistingUser(
      host_status_with_device.host_device()->name());
  pref_service_->SetInt64(kExistingUserChromebookAddedPrefName,
                          clock_->Now().InMillisecondsSinceUnixEpoch());
}

std::optional<std::string> AccountStatusChangeDelegateNotifierImpl::
    LoadHostDeviceIdFromEndOfPreviousSession() {
  std::string verified_host_device_id_from_most_recent_update =
      pref_service_->GetString(
          kVerifiedHostDeviceIdFromMostRecentHostStatusUpdatePrefName);
  if (verified_host_device_id_from_most_recent_update.empty())
    return std::nullopt;
  return verified_host_device_id_from_most_recent_update;
}

}  // namespace multidevice_setup

}  // namespace ash
