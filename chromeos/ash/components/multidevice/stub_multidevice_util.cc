// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/stub_multidevice_util.h"

#include <map>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/base64.h"
#include "base/base64url.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/beacon_seed.h"

namespace ash::multidevice {

namespace {

// Attributes of the default stub device.
const char kStubDeviceUserId[] = "example@gmail.com";
const char kStubDevicePiiFreeName[] = "no-pii device";
const char kStubDevicePSK[] = "remote device psk";
const int64_t kStubDeviceLastUpdateTimeMillis = 0L;
const char kBeaconSeedData[] = "beacon seed data";
const int64_t kBeaconSeedStartTimeMillis = 100L;
const int64_t kBeaconSeedEndTimeMillis = 200L;

}  // namespace

// Attributes of the default stub devices.
const char kStubHostPhoneName[] = "Fake Phone";
const char kStubClientComputerName[] = "Fake Computer";
const char kStubHostPhoneInstanceId[] = "1234";
const char kStubClientComputerInstanceId[] = "5678";
const char kStubHostPhonePublicKey[] = "public key phone";
const char kStubClientComputerPublicKey[] = "public key computer";
const char kStubDeviceBluetoothPublicAddress[] = "01:23:45:67:89:AB";

RemoteDevice CreateStubHostPhone() {
  static const base::NoDestructor<RemoteDevice> host_phone([] {
    // Stub host phone defaults to all host features enabled.
    std::map<SoftwareFeature, SoftwareFeatureState> software_features;
    software_features[SoftwareFeature::kBetterTogetherHost] =
        SoftwareFeatureState::kEnabled;
    software_features[SoftwareFeature::kSmartLockHost] =
        SoftwareFeatureState::kEnabled;
    software_features[SoftwareFeature::kInstantTetheringHost] =
        SoftwareFeatureState::kEnabled;
    software_features[SoftwareFeature::kMessagesForWebHost] =
        SoftwareFeatureState::kEnabled;
    software_features[SoftwareFeature::kPhoneHubHost] =
        SoftwareFeatureState::kEnabled;
    software_features[SoftwareFeature::kWifiSyncHost] =
        SoftwareFeatureState::kEnabled;
    software_features[SoftwareFeature::kEcheHost] =
        SoftwareFeatureState::kEnabled;
    software_features[SoftwareFeature::kPhoneHubCameraRollHost] =
        SoftwareFeatureState::kEnabled;

    std::vector<BeaconSeed> beacon_seeds = {multidevice::BeaconSeed(
        kBeaconSeedData,
        base::Time::FromMillisecondsSinceUnixEpoch(kBeaconSeedStartTimeMillis),
        base::Time::FromMillisecondsSinceUnixEpoch(kBeaconSeedEndTimeMillis))};

    return RemoteDevice(kStubDeviceUserId, kStubHostPhoneInstanceId,
                        kStubHostPhoneName, kStubDevicePiiFreeName,
                        kStubHostPhonePublicKey, kStubDevicePSK,
                        kStubDeviceLastUpdateTimeMillis, software_features,
                        beacon_seeds, kStubDeviceBluetoothPublicAddress);
  }());

  return *host_phone;
}

RemoteDevice CreateStubClientComputer() {
  static const base::NoDestructor<RemoteDevice> client_computer([] {
    // Stub client computer relies on flags.
    std::map<SoftwareFeature, SoftwareFeatureState> software_features;
    software_features[SoftwareFeature::kBetterTogetherClient] =
        SoftwareFeatureState::kSupported;
    software_features[SoftwareFeature::kSmartLockClient] =
        SoftwareFeatureState::kSupported;
    software_features[SoftwareFeature::kMessagesForWebClient] =
        SoftwareFeatureState::kSupported;

    software_features[SoftwareFeature::kInstantTetheringClient] =
        base::FeatureList::IsEnabled(features::kInstantTethering)
            ? SoftwareFeatureState::kSupported
            : SoftwareFeatureState::kNotSupported;

    software_features[SoftwareFeature::kPhoneHubClient] =
        features::IsPhoneHubEnabled() ? SoftwareFeatureState::kSupported
                                      : SoftwareFeatureState::kNotSupported;

    software_features[SoftwareFeature::kWifiSyncClient] =
        features::IsWifiSyncAndroidEnabled()
            ? SoftwareFeatureState::kSupported
            : SoftwareFeatureState::kNotSupported;

    software_features[SoftwareFeature::kEcheClient] =
        features::IsEcheSWAEnabled() ? SoftwareFeatureState::kSupported
                                     : SoftwareFeatureState::kNotSupported;

    software_features[SoftwareFeature::kPhoneHubCameraRollClient] =
        features::IsPhoneHubCameraRollEnabled()
            ? SoftwareFeatureState::kSupported
            : SoftwareFeatureState::kNotSupported;

    std::vector<BeaconSeed> beacon_seeds = {multidevice::BeaconSeed(
        kBeaconSeedData,
        base::Time::FromMillisecondsSinceUnixEpoch(kBeaconSeedStartTimeMillis),
        base::Time::FromMillisecondsSinceUnixEpoch(kBeaconSeedEndTimeMillis))};

    return RemoteDevice(kStubDeviceUserId, kStubClientComputerInstanceId,
                        kStubClientComputerName, kStubDevicePiiFreeName,
                        kStubClientComputerPublicKey, kStubDevicePSK,
                        kStubDeviceLastUpdateTimeMillis, software_features,
                        beacon_seeds, kStubDeviceBluetoothPublicAddress);
  }());

  return *client_computer;
}

bool ShouldUseMultideviceStubs() {
  // Should use multidevice stubs if running on Linux CrOS build which doesn't
  // support making authenticated network requests to the back-end.
  return !base::SysInfo::IsRunningOnChromeOS();
}

}  // namespace ash::multidevice
