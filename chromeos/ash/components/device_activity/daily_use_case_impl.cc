// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/daily_use_case_impl.h"

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

DailyUseCaseImpl::DailyUseCaseImpl(
    const std::string& psm_device_active_secret,
    const ChromeDeviceMetadataParameters& chrome_passed_device_params,
    PrefService* local_state,
    std::unique_ptr<PsmDelegateInterface> psm_delegate)
    : DeviceActiveUseCase(psm_device_active_secret,
                          chrome_passed_device_params,
                          prefs::kDeviceActiveLastKnownDailyPingTimestamp,
                          psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY,
                          local_state,
                          std::move(psm_delegate)) {}

DailyUseCaseImpl::~DailyUseCaseImpl() = default;

absl::optional<FresnelImportDataRequest>
DailyUseCaseImpl::GenerateImportRequestBody() {
  // Generate Fresnel PSM import request body.
  FresnelImportDataRequest import_request;

  // Create fresh |DeviceMetadata| object.
  // Note every dimension added to this proto must be approved by privacy.
  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  device_metadata->set_chromeos_version(GetChromeOSVersion());
  device_metadata->set_chromeos_channel(GetChromeOSChannel());
  device_metadata->set_market_segment(GetMarketSegment());
  device_metadata->set_hardware_id(GetFullHardwareClass());

  import_request.set_use_case(GetPsmUseCase());

  std::string psm_id_str = GetPsmIdentifier().value().sensitive_id();
  std::string window_id_str = GetWindowIdentifier().value();

  FresnelImportData* import_data = import_request.add_import_data();
  import_data->set_plaintext_id(psm_id_str);
  import_data->set_window_identifier(window_id_str);
  import_data->set_is_pt_window_identifier(true);

  return import_request;
}

bool DailyUseCaseImpl::IsEnabledCheckIn() {
  return true;
}

bool DailyUseCaseImpl::IsEnabledCheckMembership() {
  return base::FeatureList::IsEnabled(
      features::kDeviceActiveClientDailyCheckMembership);
}

private_computing::ActiveStatus DailyUseCaseImpl::GenerateActiveStatus() {
  private_computing::ActiveStatus status;

  status.set_use_case(
      private_computing::PrivateComputingUseCase::CROS_FRESNEL_DAILY);

  std::string last_ping_pt_date =
      FormatPTDateString(GetLastKnownPingTimestamp());
  status.set_last_ping_date(last_ping_pt_date);

  return status;
}

}  // namespace ash::device_activity
