// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/host_device_timestamp_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "base/time/clock.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace multidevice_setup {

namespace {
const int64_t kTimestampNotSet = 0;
}  // namespace

// static
HostDeviceTimestampManagerImpl::Factory*
    HostDeviceTimestampManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<HostDeviceTimestampManager>
HostDeviceTimestampManagerImpl::Factory::Create(
    HostStatusProvider* host_status_provider,
    PrefService* pref_service,
    base::Clock* clock) {
  if (test_factory_) {
    return test_factory_->CreateInstance(host_status_provider, pref_service,
                                         clock);
  }

  return base::WrapUnique(new HostDeviceTimestampManagerImpl(
      host_status_provider, pref_service, clock));
}

// static
void HostDeviceTimestampManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

HostDeviceTimestampManagerImpl::Factory::~Factory() = default;

// static
const char
    HostDeviceTimestampManagerImpl::kWasHostSetFromThisChromebookPrefName[] =
        "multidevice_setup.was_host_set_from_this_chromebook";

// static
const char HostDeviceTimestampManagerImpl::kSetupFlowCompletedPrefName[] =
    "multidevice_setup.setup_flow_completed";

// static
const char
    HostDeviceTimestampManagerImpl::kHostVerifiedUpdateReceivedPrefName[] =
        "multidevice_setup.host_verified_update_received";

// static
void HostDeviceTimestampManagerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kWasHostSetFromThisChromebookPrefName, false);
  registry->RegisterInt64Pref(kSetupFlowCompletedPrefName, kTimestampNotSet);
  registry->RegisterInt64Pref(kHostVerifiedUpdateReceivedPrefName,
                              kTimestampNotSet);
}

HostDeviceTimestampManagerImpl::~HostDeviceTimestampManagerImpl() {
  host_status_provider_->RemoveObserver(this);
}

HostDeviceTimestampManagerImpl::HostDeviceTimestampManagerImpl(
    HostStatusProvider* host_status_provider,
    PrefService* pref_service,
    base::Clock* clock)
    : host_status_provider_(host_status_provider),
      pref_service_(pref_service),
      clock_(clock) {
  host_status_provider_->AddObserver(this);
}

bool HostDeviceTimestampManagerImpl::WasHostSetFromThisChromebook() {
  return pref_service_->GetBoolean(kWasHostSetFromThisChromebookPrefName);
}

std::optional<base::Time>
HostDeviceTimestampManagerImpl::GetLatestSetupFlowCompletionTimestamp() {
  if (pref_service_->GetInt64(kSetupFlowCompletedPrefName) == kTimestampNotSet)
    return std::nullopt;
  return base::Time::FromMillisecondsSinceUnixEpoch(
      pref_service_->GetInt64(kSetupFlowCompletedPrefName));
}

std::optional<base::Time>
HostDeviceTimestampManagerImpl::GetLatestVerificationTimestamp() {
  if (pref_service_->GetInt64(kHostVerifiedUpdateReceivedPrefName) ==
      kTimestampNotSet)
    return std::nullopt;
  return base::Time::FromMillisecondsSinceUnixEpoch(
      pref_service_->GetInt64(kHostVerifiedUpdateReceivedPrefName));
}

void HostDeviceTimestampManagerImpl::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  // Check if setup flow was completed on this Chromebook. Note that it suffices
  // to use a host status update with the status
  // kHostSetLocallyButWaitingForBackendConfirmation as a proxy for completing
  // setup flow because the Chromebook sets a host locally (i.e. enters this
  // state) exactly when it successfully completes the flow. Note that this is
  // equivalent to a host being set on the Chromebook.
  if (host_status_with_device.host_status() ==
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation) {
    pref_service_->SetInt64(kSetupFlowCompletedPrefName,
                            clock_->Now().InMillisecondsSinceUnixEpoch());
    pref_service_->SetBoolean(kWasHostSetFromThisChromebookPrefName, true);
    PA_LOG(VERBOSE) << "HostDeviceTimestampManagerImpl::OnHostStatusChange(): "
                    << "Setup flow successfully completed. Recording timestamp "
                    << pref_service_->GetInt64(kSetupFlowCompletedPrefName)
                    << ".";
  }

  // Check if a host has been verified.
  if (host_status_with_device.host_status() ==
      mojom::HostStatus::kHostVerified) {
    pref_service_->SetInt64(kHostVerifiedUpdateReceivedPrefName,
                            clock_->Now().InMillisecondsSinceUnixEpoch());
    PA_LOG(VERBOSE) << "HostDeviceTimestampManagerImpl::OnHostStatusChange(): "
                    << "New host verified. Recording timestamp "
                    << pref_service_->GetInt64(
                           kHostVerifiedUpdateReceivedPrefName)
                    << ".";
  }

  // If there is no host set, set the "was host set form this Chromebook" bit to
  // false.
  if (host_status_with_device.host_status() ==
          mojom::HostStatus::kNoEligibleHosts ||
      host_status_with_device.host_status() ==
          mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
    pref_service_->SetBoolean(kWasHostSetFromThisChromebookPrefName, false);
  }
}

}  // namespace multidevice_setup

}  // namespace ash
