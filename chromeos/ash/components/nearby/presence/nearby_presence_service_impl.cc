// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service_impl.h"

#include "base/check.h"
#include "base/logging.h"
#include "chromeos/ash/components/nearby/presence/prefs/nearby_presence_prefs.h"
#include "components/prefs/pref_service.h"

namespace ash::nearby::presence {

NearbyPresenceServiceImpl::NearbyPresenceServiceImpl(PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
}

NearbyPresenceServiceImpl::~NearbyPresenceServiceImpl() = default;

std::unique_ptr<NearbyPresenceService::ScanSession>
NearbyPresenceServiceImpl::StartScan(ScanFilter scan_filter,
                                     ScanDelegate* scan_delegate) {
  CHECK(scan_delegate);

  // TODO(b/276359326): create the StartScan() implementation, the following is
  // only used for testing the scan delegate.
  std::unique_ptr<NearbyPresenceService::ScanSession> session;

  auto device = NearbyPresenceService::PresenceDevice(
      NearbyPresenceService::PresenceDevice::DeviceType::kChromeOS,
      /*stable_device_id_=*/"0", /*device_name_=*/"",
      /*actions_=*/{}, /*rssi_=*/1);
  scan_delegate->OnPresenceDeviceFound(device);
  return session;
}

void NearbyPresenceServiceImpl::Shutdown() {}

}  // namespace ash::nearby::presence
