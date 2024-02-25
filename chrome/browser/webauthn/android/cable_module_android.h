// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_MODULE_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_MODULE_ANDROID_H_

#include <optional>

#include "components/sync_device_info/device_info.h"

class PrefRegistrySimple;
class PrefService;

namespace webauthn {
namespace authenticator {

// RegisterForCloudMessages installs a |GCMAppHandler| that handles caBLEv2
// message in the |GCMDriver| connected to the primary profile. This should be
// called during browser startup to ensure that the |GCMAppHandler| is
// registered before any GCM messages are processed. (Otherwise they will be
// dropped.)
void RegisterForCloudMessages();

// GetSyncDataIfRegistered returns a structure containing values to advertise
// in Sync that will let other Chrome instances contact this device to perform
// security key transactions.
syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo
GetSyncDataIfRegistered();

// RegisterLocalState registers prefs with the local-state represented by
// |registry|.
void RegisterLocalState(PrefRegistrySimple* registry);

namespace internal {

// PaaskInfoFromCBOR parses a CBOR-encoded linking structure from Play Services
// into the structure used by Sync.
std::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo> PaaskInfoFromCBOR(
    base::span<const uint8_t> cbor);

// CBORFromPaaskInfo does the inverse of `PaaskInfoFromCBOR`.
std::vector<uint8_t> CBORFromPaaskInfo(
    const syncer::DeviceInfo::PhoneAsASecurityKeyInfo& paask_info);

// CacheResult will save `result`, if it's not `NotReady`, into `state`. If it
// is `NotReady`, it'll try to load a previously saved result and will return
// that instead.
syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo CacheResult(
    syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo result,
    PrefService* state);

}  // namespace internal

}  // namespace authenticator
}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_ANDROID_CABLE_MODULE_ANDROID_H_
