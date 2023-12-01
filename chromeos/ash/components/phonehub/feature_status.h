// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_H_

#include <ostream>

namespace ash::phonehub {

// Enum representing potential status values for the Phone Hub feature. Note
// that there is no value representing "prohibited" - when the feature is
// prohibited by enterprise policy, we don't instantiate Phone Hub-related logic
// at all.
// Note: This enum is tied directly to the PhoneHubFeatureStatus enum defined in
// //tools/metrics/histograms/metadata/phonehub/enums.xml, and should always
// reflect it (do not change one without changing the other). Entries should
// never be modified or deleted. Only additions possible.
enum class FeatureStatus {
  // The user's devices are not eligible for the feature. This means that either
  // the Chrome OS device or the user's phone (or both) have not enrolled with
  // the requisite feature enum values.
  kNotEligibleForFeature = 0,

  // The user has a phone eligible for the feature, but they have not yet
  // started the opt-in flow.
  kEligiblePhoneButNotSetUp = 1,

  // The user has selected a phone in the opt-in flow, but setup is not yet
  // complete. Note that setting up the feature requires interaction with a
  // server and with the phone itself.
  kPhoneSelectedAndPendingSetup = 2,

  // The feature is disabled, but the user could enable it via settings.
  kDisabled = 3,

  // The feature is enabled, but it is currently unavailable because Bluetooth
  // is disabled (the feature cannot run without Bluetooth).
  kUnavailableBluetoothOff = 4,

  // The feature is enabled, but currently there is no active connection to
  // the phone.
  kEnabledButDisconnected = 5,

  // The feature is enabled, and there is an active attempt to connect to the
  // phone.
  kEnabledAndConnecting = 6,

  // The feature is enabled, and there is an active connection with the phone.
  kEnabledAndConnected = 7,

  // The feature is unavailable because the device is in a suspended state. This
  // includes the having either the lockscreen active or in a power suspend
  // state, e.g. lid closed.
  kLockOrSuspended = 8,

  // Max value needed for metrics.
  kMaxValue = kLockOrSuspended,
};

std::ostream& operator<<(std::ostream& stream, FeatureStatus status);

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_H_
