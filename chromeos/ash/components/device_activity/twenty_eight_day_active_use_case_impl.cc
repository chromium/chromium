// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/twenty_eight_day_active_use_case_impl.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/device_activity/fresnel_service.pb.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Used to configure the last N days to send plaintext id check membership
// requests.
constexpr size_t kRollingWindowSize = 28;

}  // namespace

TwentyEightDayActiveUseCaseImpl::TwentyEightDayActiveUseCaseImpl(
    const std::string& psm_device_active_secret,
    const ChromeDeviceMetadataParameters& chrome_passed_device_params,
    PrefService* local_state,
    std::unique_ptr<PsmDelegateInterface> psm_delegate)
    : DeviceActiveUseCase(psm_device_active_secret,
                          chrome_passed_device_params,
                          prefs::kDeviceActiveLastKnown28DayActivePingTimestamp,
                          psm_rlwe::RlweUseCase::CROS_FRESNEL_28DAY_ACTIVE,
                          local_state,
                          std::move(psm_delegate)) {}

TwentyEightDayActiveUseCaseImpl::~TwentyEightDayActiveUseCaseImpl() = default;

std::string TwentyEightDayActiveUseCaseImpl::GenerateUTCWindowIdentifier(
    base::Time ts) const {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);
  return base::StringPrintf("%04d%02d%02d", exploded.year, exploded.month,
                            exploded.day_of_month);
}

FresnelImportDataRequest
TwentyEightDayActiveUseCaseImpl::GenerateImportRequestBody() {
  // Generate Fresnel PSM import request body.
  FresnelImportDataRequest import_request;

  import_request.set_use_case(GetPsmUseCase());

  // Create fresh |DeviceMetadata| object.
  // Note every dimension added to this proto must be approved by privacy.
  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  device_metadata->set_chromeos_version(GetChromeOSVersion());
  device_metadata->set_chromeos_channel(GetChromeOSChannel());

  // Enable sending these during 28DA check in
  // without check membership for debugging purposes.
  device_metadata->set_hardware_id(GetFullHardwareClass());
  device_metadata->set_market_segment(GetMarketSegment());

  for (auto v : new_import_data_) {
    FresnelImportData* import_data = import_request.add_import_data();
    import_data->set_window_identifier(v.window_identifier());
    import_data->set_plaintext_id(v.plaintext_id());
  }

  return import_request;
}

bool TwentyEightDayActiveUseCaseImpl::SavePsmIdToDateMap(base::Time cur_ts) {
  // Generate |kRollingWindowSize| days of PSM identifiers to search.
  std::unordered_map<std::string, base::Time> psm_id_to_date_temp;

  for (int i = 0; i < static_cast<int>(kRollingWindowSize); i++) {
    base::Time day_n = cur_ts - base::Days(i);

    absl::optional<psm_rlwe::RlwePlaintextId> id =
        GeneratePsmIdentifier(GenerateUTCWindowIdentifier(day_n));

    if (!id.has_value()) {
      LOG(ERROR) << "PSM ID is empty";
      return false;
    }

    psm_id_to_date_temp.insert(
        {id.value().sensitive_id(), day_n.UTCMidnight()});
  }

  psm_id_to_date_ = psm_id_to_date_temp;
  return true;
}

bool TwentyEightDayActiveUseCaseImpl::SetPsmIdentifiersToImport(
    base::Time cur_ts) {
  DCHECK(psm_id_.has_value());

  // Clear previous values of id's to import.
  new_import_data_.clear();

  base::Time last_known_ping_ts = GetLastKnownPingTimestamp();
  for (int i = 0; i < static_cast<int>(kRollingWindowSize); i++) {
    base::Time day_n = cur_ts + base::Days(i);

    // Only generate import data for new identifiers to import.
    if (day_n < (last_known_ping_ts + base::Days(kRollingWindowSize)))
      continue;

    std::string window_id = GenerateUTCWindowIdentifier(day_n);
    absl::optional<psm_rlwe::RlwePlaintextId> id =
        GeneratePsmIdentifier(window_id);

    if (window_id.empty() || !id.has_value()) {
      LOG(ERROR) << "Window id or PSM ID is empty.";
      return false;
    }

    FresnelImportData import_data = FresnelImportData();
    import_data.set_window_identifier(window_id);
    import_data.set_plaintext_id(id.value().sensitive_id());

    new_import_data_.push_back(import_data);
  }

  return true;
}

}  // namespace ash::device_activity
