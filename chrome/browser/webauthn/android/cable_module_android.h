// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_MODULE_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_MODULE_ANDROID_H_

#include "components/sync_device_info/device_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;

namespace webauthn {
namespace authenticator {

// RegisterForCloudMessages installs a |GCMAppHandler| that handles caBLEv2
// message in the |GCMDriver| connected to the primary profile. This should be
// called during browser startup to ensure that the |GCMAppHandler| is
// registered before any GCM messages are processed. (Otherwise they will be
// dropped.)
void RegisterForCloudMessages();

// GetSyncDataIfRegistered returns a structure containing values to advertise in
// Sync that will let other Chrome instances contact this device to perform
// security key transactions, or it returns |nullopt| if that information is
// not yet ready.
absl::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>
GetSyncDataIfRegistered();

// RegisterLocalState registers prefs with the local-state represented by
// |registry|.
void RegisterLocalState(PrefRegistrySimple* registry);

}  // namespace authenticator
}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_MODULE_ANDROID_H_
