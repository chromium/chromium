// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace {

constexpr base::TimeDelta kInitialFeatureStateLoggingDelay = base::Seconds(15);
constexpr base::TimeDelta kRepeatingFeatureStateLoggingPeriod =
    base::Minutes(30);

// Log the feature states in |feature_states_map|. Called 1) on
// sign-in, 2) when at least one feature state changes, and 3) every 30
// minutes. The latter is necessary to capture users who stay logged in longer
// than UMA aggregation periods and don't change feature state.
void LogFeatureStates(
    const ash::multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  // There is a duplicate metric of
  // "MultiDevice.BetterTogetherSuite.MultiDeviceFeatureState" on different
  // ends of the mojo pipe to help us understand how frequent we get stuck
  // on this end of the pipe waiting for the client to be ready. See b/215469053
  // for more context.
  base::UmaHistogramEnumeration(
      "MultiDevice.BetterTogetherSuite.MultiDeviceFeatureState.MojoClient",
      feature_states_map
          .find(ash::multidevice_setup::mojom::Feature::kBetterTogetherSuite)
          ->second);
}

}  // namespace

namespace ash {

namespace multidevice_setup {

// static
MultiDeviceSetupClientImpl::Factory*
    MultiDeviceSetupClientImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<MultiDeviceSetupClient>
MultiDeviceSetupClientImpl::Factory::Create(
    mojo::PendingRemote<mojom::MultiDeviceSetup> remote_setup) {
  if (test_factory_)
    return test_factory_->CreateInstance(std::move(remote_setup));

  return base::WrapUnique(
      new MultiDeviceSetupClientImpl(std::move(remote_setup)));
}

// static
void MultiDeviceSetupClientImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

MultiDeviceSetupClientImpl::Factory::~Factory() = default;

MultiDeviceSetupClientImpl::MultiDeviceSetupClientImpl(
    mojo::PendingRemote<mojom::MultiDeviceSetup> remote_setup)
    : multidevice_setup_remote_(std::move(remote_setup)),
      remote_device_cache_(multidevice::RemoteDeviceCache::Factory::Create()),
      host_status_with_device_(GenerateDefaultHostStatusWithDevice()),
      feature_states_map_(GenerateDefaultFeatureStatesMap(
          mojom::FeatureState::kUnavailableNoVerifiedHost_ClientNotReady)) {
  multidevice_setup_remote_->AddHostStatusObserver(
      GenerateHostStatusObserverRemote());
  multidevice_setup_remote_->AddFeatureStateObserver(
      GenerateFeatureStatesObserverRemote());
  multidevice_setup_remote_->GetHostStatus(
      base::BindOnce(&MultiDeviceSetupClientImpl::OnHostStatusChanged,
                     base::Unretained(this)));
  multidevice_setup_remote_->GetFeatureStates(
      base::BindOnce(&MultiDeviceSetupClientImpl::OnFeatureStatesChanged,
                     base::Unretained(this)));

  // Delay the initial feature metric state logger in order to allow for
  // the client to become ready on initialization before we log, otherwise the
  // |kUnavailableNoVerifiedHost_ClientNotReady| will always be logged, which
  // does not help us understand the frequency of getting "stuck" waiting for
  // the client. The timer ends sooner than |kInitialFeatureStateLoggingDelay|
  // when |OnFeatureStatesChanged| is called (which signals the client is
  // ready and is logged).
  initial_feature_state_metric_logging_timer_.Start(
      FROM_HERE, kInitialFeatureStateLoggingDelay,
      base::BindRepeating(
          &MultiDeviceSetupClientImpl::OnFeatureStateMetricTimerFired,
          base::Unretained(this)));

  // Log the feature states every |kRepeatingFeatureStateLoggingPeriod| to
  // capture users who stay logged in longer than UMA aggregation periods and
  // don't change feature state. For more information on the frequency of
  // logging, see comments above `LogFeatureStates()`.
  feature_state_metric_timer_.Start(
      FROM_HERE, kRepeatingFeatureStateLoggingPeriod,
      base::BindRepeating(
          &MultiDeviceSetupClientImpl::OnFeatureStateMetricTimerFired,
          base::Unretained(this)));
}

MultiDeviceSetupClientImpl::~MultiDeviceSetupClientImpl() = default;

void MultiDeviceSetupClientImpl::OnFeatureStateMetricTimerFired() {
  LogFeatureStates(feature_states_map_);
}

void MultiDeviceSetupClientImpl::GetEligibleHostDevices(
    GetEligibleHostDevicesCallback callback) {
  multidevice_setup_remote_->GetEligibleHostDevices(base::BindOnce(
      &MultiDeviceSetupClientImpl::OnGetEligibleHostDevicesCompleted,
      base::Unretained(this), std::move(callback)));
}

void MultiDeviceSetupClientImpl::SetHostDevice(
    const std::string& host_instance_id_or_legacy_device_id,
    const std::string& auth_token,
    mojom::MultiDeviceSetup::SetHostDeviceCallback callback) {
  multidevice_setup_remote_->SetHostDevice(host_instance_id_or_legacy_device_id,
                                           auth_token, std::move(callback));
}

void MultiDeviceSetupClientImpl::RemoveHostDevice() {
  multidevice_setup_remote_->RemoveHostDevice();
}

const MultiDeviceSetupClient::HostStatusWithDevice&
MultiDeviceSetupClientImpl::GetHostStatus() const {
  PA_LOG(INFO) << "Responding to GetHostStatus() with the following host = "
               << HostStatusWithDeviceToString(host_status_with_device_);
  return host_status_with_device_;
}

void MultiDeviceSetupClientImpl::SetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled,
    const std::optional<std::string>& auth_token,
    mojom::MultiDeviceSetup::SetFeatureEnabledStateCallback callback) {
  multidevice_setup_remote_->SetFeatureEnabledState(
      feature, enabled, auth_token, std::move(callback));
}

const MultiDeviceSetupClient::FeatureStatesMap&
MultiDeviceSetupClientImpl::GetFeatureStates() const {
  return feature_states_map_;
}

void MultiDeviceSetupClientImpl::RetrySetHostNow(
    mojom::MultiDeviceSetup::RetrySetHostNowCallback callback) {
  multidevice_setup_remote_->RetrySetHostNow(std::move(callback));
}

void MultiDeviceSetupClientImpl::TriggerEventForDebugging(
    mojom::EventTypeForDebugging type,
    mojom::MultiDeviceSetup::TriggerEventForDebuggingCallback callback) {
  multidevice_setup_remote_->TriggerEventForDebugging(type,
                                                      std::move(callback));
}

void MultiDeviceSetupClientImpl::SetQuickStartPhoneInstanceID(
    const std::string& qs_phone_instance_id) {
  multidevice_setup_remote_->SetQuickStartPhoneInstanceID(qs_phone_instance_id);
}

void MultiDeviceSetupClientImpl::OnHostStatusChanged(
    mojom::HostStatus host_status,
    const std::optional<multidevice::RemoteDevice>& host_device) {
  if (host_device) {
    remote_device_cache_->SetRemoteDevices({*host_device});
    host_status_with_device_ = std::make_pair(
        host_status, remote_device_cache_->GetRemoteDevice(
                         host_device->instance_id, host_device->GetDeviceId()));
  } else {
    host_status_with_device_ =
        std::make_pair(host_status, std::nullopt /* host_device */);
  }

  PA_LOG(INFO) << "Host status with device has changed. New status: "
               << HostStatusWithDeviceToString(host_status_with_device_);
  NotifyHostStatusChanged(host_status_with_device_);
}

void MultiDeviceSetupClientImpl::OnFeatureStatesChanged(
    const FeatureStatesMap& feature_states_map) {
  initial_feature_state_metric_logging_timer_.Stop();
  PA_LOG(INFO) << "Feature states have changed. New feature map: "
               << FeatureStatesMapToString(feature_states_map);
  feature_states_map_ = feature_states_map;
  NotifyFeatureStateChanged(feature_states_map_);
  LogFeatureStates(feature_states_map_);
}

void MultiDeviceSetupClientImpl::OnGetEligibleHostDevicesCompleted(
    GetEligibleHostDevicesCallback callback,
    const multidevice::RemoteDeviceList& eligible_host_devices) {
  remote_device_cache_->SetRemoteDevices(eligible_host_devices);

  multidevice::RemoteDeviceRefList eligible_host_device_refs;
  base::ranges::transform(eligible_host_devices,
                          std::back_inserter(eligible_host_device_refs),
                          [this](const auto& device) {
                            return *remote_device_cache_->GetRemoteDevice(
                                device.instance_id, device.GetDeviceId());
                          });

  std::move(callback).Run(eligible_host_device_refs);
}

mojo::PendingRemote<mojom::HostStatusObserver>
MultiDeviceSetupClientImpl::GenerateHostStatusObserverRemote() {
  return host_status_observer_receiver_.BindNewPipeAndPassRemote();
}

mojo::PendingRemote<mojom::FeatureStateObserver>
MultiDeviceSetupClientImpl::GenerateFeatureStatesObserverRemote() {
  return feature_state_observer_receiver_.BindNewPipeAndPassRemote();
}

void MultiDeviceSetupClientImpl::FlushForTesting() {
  multidevice_setup_remote_.FlushForTesting();
}

}  // namespace multidevice_setup

}  // namespace ash
