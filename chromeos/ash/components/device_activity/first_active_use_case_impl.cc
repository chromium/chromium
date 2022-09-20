// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/first_active_use_case_impl.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/device_activity/fresnel_service.pb.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {

namespace psm_rlwe = private_membership::rlwe;

FirstActiveUseCaseImpl::FirstActiveUseCaseImpl(
    const std::string& psm_device_active_secret,
    const ChromeDeviceMetadataParameters& chrome_passed_device_params,
    PrefService* local_state)
    : DeviceActiveUseCase(psm_device_active_secret,
                          chrome_passed_device_params,
                          prefs::kDeviceActiveLastKnownFirstActivePingTimestamp,
                          psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE,
                          local_state) {}

FirstActiveUseCaseImpl::~FirstActiveUseCaseImpl() = default;

std::string FirstActiveUseCaseImpl::GenerateUTCWindowIdentifier(
    base::Time ts) const {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);
  return base::StringPrintf("%04d%02d%02d", exploded.year, exploded.month,
                            exploded.day_of_month);
}

ImportDataRequest FirstActiveUseCaseImpl::GenerateImportRequestBody() {
  std::string psm_id_str = GetPsmIdentifier().value().sensitive_id();
  std::string window_id_str = GetWindowIdentifier().value();

  // Generate Fresnel PSM import request body.
  device_activity::ImportDataRequest import_request;
  import_request.set_window_identifier(window_id_str);
  import_request.set_plaintext_identifier(psm_id_str);
  import_request.set_use_case(GetPsmUseCase());

  // Create fresh |DeviceMetadata| object.
  // Note every dimension added to this proto must be approved by privacy.
  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  device_metadata->set_chromeos_version(GetChromeOSVersion());
  device_metadata->set_chromeos_channel(GetChromeOSChannel());

  // TODO(hirthanan): Enable when rolling out check membership requests for the
  // first active use case.
  // device_metadata->set_hardware_id(GetFullHardwareClass());
  // device_metadata->set_market_segment(GetMarketSegment());

  return import_request;
}

}  // namespace device_activity
}  // namespace ash
