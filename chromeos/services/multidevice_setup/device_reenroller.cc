// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/device_reenroller.h"

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "components/cryptauth/gcm_device_info_provider.h"
#include "components/cryptauth/proto/enum_util.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

// The number of minutes to wait before retrying a failed re-enrollment attempt.
const int kNumMinutesBetweenRetries = 5;

// Returns a sorted and deduped list of the supported software features from
// GcmDeviceInfo.
std::vector<cryptauth::SoftwareFeature> GetSupportedFeaturesFromGcmDeviceInfo(
    const cryptauth::GcmDeviceInfo& gcm_device_info) {
  base::flat_set<cryptauth::SoftwareFeature> sorted_and_deduped_set;
  for (int i = 0; i < gcm_device_info.supported_software_features_size(); ++i) {
    sorted_and_deduped_set.insert(
        gcm_device_info.supported_software_features(i));
  }
  return std::vector<cryptauth::SoftwareFeature>(sorted_and_deduped_set.begin(),
                                                 sorted_and_deduped_set.end());
}

void OnForceEnrollmentNow(bool success) {
  if (success) {
    PA_LOG(VERBOSE) << "Forced enrollment was successfully requested.";
    return;
  }
  PA_LOG(WARNING) << "Forced enrollment was not successfully requested. "
                  << "Waiting for " << kNumMinutesBetweenRetries << "-minute "
                  << "re-enrollment retry timer to fire.";
}

void OnForceSyncNow(bool success) {
  if (success) {
    PA_LOG(VERBOSE) << "Forced device sync was successfully requested.";
    return;
  }
  PA_LOG(WARNING) << "Forced device sync was not successfully requested. "
                  << "Waiting for " << kNumMinutesBetweenRetries << "-minute "
                  << "re-enrollment retry timer to fire.";
}

std::string CreateSoftwareFeaturesString(
    const std::vector<cryptauth::SoftwareFeature>& software_features) {
  std::stringstream ss;
  for (cryptauth::SoftwareFeature feature : software_features) {
    ss << feature << " ";
  }
  return ss.str();
}

}  // namespace

// static
DeviceReenroller::Factory* DeviceReenroller::Factory::test_factory_ = nullptr;

// static
DeviceReenroller::Factory* DeviceReenroller::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void DeviceReenroller::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

DeviceReenroller::Factory::~Factory() = default;

std::unique_ptr<DeviceReenroller> DeviceReenroller::Factory::BuildInstance(
    device_sync::DeviceSyncClient* device_sync_client,
    const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(new DeviceReenroller(
      device_sync_client, gcm_device_info_provider, std::move(timer)));
}

DeviceReenroller::~DeviceReenroller() {
  device_sync_client_->RemoveObserver(this);
}

DeviceReenroller::DeviceReenroller(
    device_sync::DeviceSyncClient* device_sync_client,
    const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider,
    std::unique_ptr<base::OneShotTimer> timer)
    : device_sync_client_(device_sync_client),
      gcm_supported_software_features_(GetSupportedFeaturesFromGcmDeviceInfo(
          gcm_device_info_provider->GetGcmDeviceInfo())),
      timer_(std::move(timer)) {
  DCHECK(device_sync_client_->is_ready());
  device_sync_client_->AddObserver(this);
  AttemptReenrollmentIfNecessary();
}

void DeviceReenroller::AttemptReenrollmentIfNecessary() {
  std::vector<cryptauth::SoftwareFeature> metadata_supported_software_features =
      GetSupportedFeaturesForLocalDevice();

  if (gcm_supported_software_features_ ==
      metadata_supported_software_features) {
    PA_LOG(VERBOSE) << "The supported software features of local device "
                    << "metadata agree with those of GCM device info. No "
                    << "further action taken.";
    return;
  }

  PA_LOG(INFO)
      << "Supported software feature mismatch. Attempting re-enrollment now."
      << std::endl
      << "    ---GcmDeviceInfo Supported Software Features---" << std::endl
      << "    "
      << CreateSoftwareFeaturesString(gcm_supported_software_features_)
      << std::endl
      << "    ---Local Device Metadata Supported Software Features---"
      << std::endl
      << "    "
      << CreateSoftwareFeaturesString(metadata_supported_software_features);
  // Attempt re-enrollment now and schedule a check-up in 5 minutes.
  device_sync_client_->ForceEnrollmentNow(
      base::BindOnce(&OnForceEnrollmentNow));
  timer_->Start(
      FROM_HERE, base::TimeDelta::FromMinutes(kNumMinutesBetweenRetries),
      base::BindOnce(&DeviceReenroller::AttemptReenrollmentIfNecessary,
                     base::Unretained(this)));
}

std::vector<cryptauth::SoftwareFeature>
DeviceReenroller::GetSupportedFeaturesForLocalDevice() {
  const cryptauth::RemoteDeviceRef local_device_metadata =
      *device_sync_client_->GetLocalDeviceMetadata();

  base::flat_set<cryptauth::SoftwareFeature> sorted_and_deduped_set;
  for (int i = cryptauth::SoftwareFeature_MIN;
       i <= cryptauth::SoftwareFeature_MAX; ++i) {
    cryptauth::SoftwareFeature feature =
        static_cast<cryptauth::SoftwareFeature>(i);
    if (local_device_metadata.GetSoftwareFeatureState(feature) !=
        cryptauth::SoftwareFeatureState::kNotSupported) {
      sorted_and_deduped_set.insert(feature);
    }
  }
  return std::vector<cryptauth::SoftwareFeature>(sorted_and_deduped_set.begin(),
                                                 sorted_and_deduped_set.end());
}

void DeviceReenroller::OnEnrollmentFinished() {
  // Only sync devices if the features disagree. This is important to check
  // because OnEnrollmentFinished() might be called due to an enrollment that
  // was not started in DeviceReenroller.
  if (gcm_supported_software_features_ ==
      GetSupportedFeaturesForLocalDevice()) {
    return;
  }
  PA_LOG(VERBOSE) << "An enrollment finished. Syncing now.";
  device_sync_client_->ForceSyncNow(base::BindOnce(&OnForceSyncNow));
}

void DeviceReenroller::OnNewDevicesSynced() {
  PA_LOG(VERBOSE) << "A device sync finished. Waiting for verification.";
  // If the retry timer is running, then wait for it to call
  // AttemptReenrollmentIfNecessary(); otherwise, call it immediately. The timer
  // should be running if a re-enrollment process that was triggered by
  // DeviceReenroller (not externally) is still in progress. So, we do not want
  // to spam the server with immediate retries should the features still not
  // agree.
  if (timer_->IsRunning()) {
    return;
  }
  AttemptReenrollmentIfNecessary();
}

}  // namespace multidevice_setup

}  // namespace chromeos
