// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/monthly_use_case_impl.h"

#include "ash/constants/ash_features.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/device_activity/fresnel_service.pb.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

MonthlyUseCaseImpl::MonthlyUseCaseImpl(
    const std::string& psm_device_active_secret,
    const ChromeDeviceMetadataParameters& chrome_passed_device_params,
    PrefService* local_state,
    std::unique_ptr<PsmDelegateInterface> psm_delegate)
    : DeviceActiveUseCase(psm_device_active_secret,
                          chrome_passed_device_params,
                          prefs::kDeviceActiveLastKnownMonthlyPingTimestamp,
                          psm_rlwe::RlweUseCase::CROS_FRESNEL_MONTHLY,
                          local_state,
                          std::move(psm_delegate)) {}

MonthlyUseCaseImpl::~MonthlyUseCaseImpl() = default;

std::string MonthlyUseCaseImpl::GenerateUTCWindowIdentifier(
    base::Time ts) const {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);
  return base::StringPrintf("%04d%02d", exploded.year, exploded.month);
}

FresnelImportDataRequest MonthlyUseCaseImpl::GenerateImportRequestBody() {
  std::string psm_id_str = GetPsmIdentifier().value().sensitive_id();
  std::string window_id_str = GetWindowIdentifier().value();

  // Generate Fresnel PSM import request body.
  FresnelImportDataRequest import_request;
  import_request.set_window_identifier(window_id_str);

  // Create fresh |DeviceMetadata| object.
  // Note every dimension added to this proto must be approved by privacy.
  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  device_metadata->set_chromeos_version(GetChromeOSVersion());
  device_metadata->set_chromeos_channel(GetChromeOSChannel());

  if (base::FeatureList::IsEnabled(
          features::kDeviceActiveClientMonthlyCheckMembership)) {
    device_metadata->set_market_segment(GetMarketSegment());
    device_metadata->set_hardware_id(GetFullHardwareClass());
  }

  import_request.set_use_case(GetPsmUseCase());
  import_request.set_plaintext_identifier(psm_id_str);

  return import_request;
}

}  // namespace ash::device_activity
