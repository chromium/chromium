// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/device_active_use_case.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "crypto/hmac.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Default value for devices that are missing the hardware class.
const char kHardwareClassKeyNotFound[] = "HARDWARE_CLASS_KEY_NOT_FOUND";

}  // namespace

DeviceActiveUseCase::DeviceActiveUseCase(
    const std::string& psm_device_active_secret,
    const ChromeDeviceMetadataParameters& chrome_passed_device_params,
    const std::string& use_case_pref_key,
    psm_rlwe::RlweUseCase psm_use_case,
    PrefService* local_state,
    std::unique_ptr<PsmDelegateInterface> psm_delegate)
    : psm_device_active_secret_(psm_device_active_secret),
      chrome_passed_device_params_(chrome_passed_device_params),
      use_case_pref_key_(use_case_pref_key),
      psm_use_case_(psm_use_case),
      local_state_(local_state),
      psm_delegate_(std::move(psm_delegate)),
      statistics_provider_(
          chromeos::system::StatisticsProvider::GetInstance()) {
  DCHECK(psm_delegate_);
  DCHECK(local_state_);
}

DeviceActiveUseCase::~DeviceActiveUseCase() = default;

void DeviceActiveUseCase::ClearSavedState() {
  window_id_ = absl::nullopt;

  psm_id_ = absl::nullopt;

  psm_id_to_date_.clear();

  psm_ids_to_query_.clear();

  psm_rlwe_client_.reset();
}

PrefService* DeviceActiveUseCase::GetLocalState() const {
  return local_state_;
}

base::Time DeviceActiveUseCase::GetLastKnownPingTimestamp() const {
  return GetLocalState()->GetTime(use_case_pref_key_);
}

void DeviceActiveUseCase::SetLastKnownPingTimestamp(base::Time new_ts) {
  GetLocalState()->SetTime(use_case_pref_key_, new_ts);
}

bool DeviceActiveUseCase::IsLastKnownPingTimestampSet() const {
  return GetLastKnownPingTimestamp() != base::Time::UnixEpoch();
}

psm_rlwe::RlweUseCase DeviceActiveUseCase::GetPsmUseCase() const {
  return psm_use_case_;
}

absl::optional<std::string> DeviceActiveUseCase::GetWindowIdentifier() const {
  return window_id_;
}

bool DeviceActiveUseCase::SetWindowIdentifier(base::Time ts) {
  std::string window_id = GenerateUTCWindowIdentifier(ts);
  psm_id_ = GeneratePsmIdentifier(window_id);

  // Check if |psm_id_| is generated.
  if (!psm_id_.has_value()) {
    LOG(ERROR) << "PSM ID has no value.";
    return false;
  }

  if (!SavePsmIdToDateMap(ts)) {
    LOG(ERROR) << "Failed to save PSM identifiers date map for ts = " << ts;
    return false;
  }

  SetPsmIdentifiersToQuery();

  if (!SetPsmIdentifiersToImport(ts)) {
    LOG(ERROR) << "Failed to set PSM identifiers to import for ts = " << ts;
    return false;
  }

  window_id_ = window_id;
  return true;
}

absl::optional<psm_rlwe::RlwePlaintextId>
DeviceActiveUseCase::GetPsmIdentifier() const {
  return psm_id_;
}

bool DeviceActiveUseCase::SavePsmIdToDateMap(base::Time ts) {
  DCHECK(psm_id_.has_value());
  psm_id_to_date_.clear();
  psm_id_to_date_.insert({psm_id_.value().sensitive_id(), ts.UTCMidnight()});
  return true;
}

void DeviceActiveUseCase::SetPsmIdentifiersToQuery() {
  // Clear previous values of id's to query.
  psm_ids_to_query_.clear();

  // Uses |psm_id_to_date_| keys to generate the psm id's to query.
  for (auto kv : psm_id_to_date_) {
    psm_rlwe::RlwePlaintextId psm_rlwe_id;
    psm_rlwe_id.set_sensitive_id(kv.first);
    psm_ids_to_query_.push_back(psm_rlwe_id);
  }
}

bool DeviceActiveUseCase::SetPsmIdentifiersToImport(base::Time ts) {
  DCHECK(psm_id_.has_value());

  // Clear previous values of id's to import.
  new_import_data_.clear();

  std::string window_id = GenerateUTCWindowIdentifier(ts);
  FresnelImportData import_data = FresnelImportData();
  import_data.set_window_identifier(window_id);
  import_data.set_plaintext_id(psm_id_.value().sensitive_id());

  return true;
}

std::vector<private_membership::rlwe::RlwePlaintextId>
DeviceActiveUseCase::GetPsmIdentifiersToQuery() const {
  return psm_ids_to_query_;
}

std::vector<FresnelImportData> DeviceActiveUseCase::GetImportData() const {
  return new_import_data_;
}

base::Time DeviceActiveUseCase::RetrievePsmIdDate(
    private_membership::rlwe::RlwePlaintextId id) {
  if (psm_id_to_date_.find(id.sensitive_id()) == psm_id_to_date_.end())
    return base::Time::UnixEpoch();
  return psm_id_to_date_.at(id.sensitive_id());
}

std::string DeviceActiveUseCase::GetDigestString(
    const std::string& key,
    const std::string& message) const {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  std::vector<uint8_t> digest(hmac.DigestLength());
  if (!hmac.Init(key) || !hmac.Sign(message, &digest[0], digest.size())) {
    return std::string();
  }
  return base::HexEncode(&digest[0], digest.size());
}

psm_rlwe::PrivateMembershipRlweClient* DeviceActiveUseCase::GetPsmRlweClient() {
  return psm_rlwe_client_.get();
}

bool DeviceActiveUseCase::IsDevicePingRequired(base::Time new_ping_ts) const {
  // Check the last recorded ping timestamp in local state prefs.
  // This variable has the default Unix Epoch value if the device is
  // new, powerwashed, recovered, or a RMA device.
  base::Time prev_ping_ts = GetLastKnownPingTimestamp();

  std::string prev_ping_window_id = GenerateUTCWindowIdentifier(prev_ping_ts);
  std::string new_ping_window_id = GenerateUTCWindowIdentifier(new_ping_ts);

  // Safety check to avoid against clock drift, or unexpected timestamps.
  // Check should make sure that we are not reporting window id's for
  // day's previous to one that we reported already.
  return prev_ping_ts < new_ping_ts &&
         prev_ping_window_id != new_ping_window_id;
}

void DeviceActiveUseCase::SetPsmRlweClient(
    std::vector<psm_rlwe::RlwePlaintextId> psm_ids) {
  DCHECK(!psm_ids.empty());

  auto status_or_client =
      psm_delegate_->CreatePsmClient(GetPsmUseCase(), psm_ids);

  if (!status_or_client.ok()) {
    LOG(ERROR) << "Failed to initialize PSM client.";
    return;
  }

  // Re-assigning the unique_ptr will reset the old unique_ptr.
  psm_rlwe_client_ = std::move(status_or_client.value());
}

bool DeviceActiveUseCase::EncryptPsmValueAsCiphertext(base::Time ts) {
  (void)ts;
  NOTREACHED();
  return false;
}

base::Time DeviceActiveUseCase::DecryptPsmValueAsTimestamp(
    std::string ciphertext) const {
  (void)ciphertext;
  NOTREACHED();
  return base::Time::UnixEpoch();
}

std::string DeviceActiveUseCase::FormatUTCDateString(base::Time ts) {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);
  return base::StringPrintf("%04d-%02d-%02d %02d:%02d:%02d.%03d UTC",
                            exploded.year, exploded.month,
                            exploded.day_of_month,
                            /* hour */ 0,
                            /* minute */ 0,
                            /* second */ 0,
                            /* millisecond */ 0);
}

absl::optional<psm_rlwe::RlwePlaintextId>
DeviceActiveUseCase::GeneratePsmIdentifier(
    absl::optional<std::string> window_id) const {
  const std::string psm_use_case = psm_rlwe::RlweUseCase_Name(GetPsmUseCase());
  if (psm_device_active_secret_.empty() || psm_use_case.empty() ||
      !window_id.has_value()) {
    VLOG(1) << "Can not generate PSM id without the psm device secret, use "
               "case, and window id being defined.";
    return absl::nullopt;
  }

  std::string unhashed_psm_id =
      base::JoinString({psm_use_case, window_id.value()}, "|");

  // |psm_id_str| represents a 64 byte hex encoded value by default.
  // However for the first active use case, this value is a 32 byte string.
  std::string psm_id_str =
      GetDigestString(psm_device_active_secret_, unhashed_psm_id);

  if (!psm_id_str.empty()) {
    psm_rlwe::RlwePlaintextId psm_rlwe_id;
    psm_rlwe_id.set_sensitive_id(psm_id_str);

    return psm_rlwe_id;
  }

  // Failed HMAC-SHA256 hash on PSM id.
  VLOG(1) << "Failed to calculate HMAC-256 has on PSM id.";
  return absl::nullopt;
}

std::string DeviceActiveUseCase::GetFullHardwareClass() const {
  // Retrieve full hardware class from machine statistics object.
  // Default |full_hardware_class| to kHardwareClassKeyNotFound if retrieval
  // from machine statistics fails.
  const absl::optional<base::StringPiece> full_hardware_class =
      statistics_provider_->GetMachineStatistic(
          chromeos::system::kHardwareClassKey);
  return std::string(full_hardware_class.value_or(kHardwareClassKeyNotFound));
}

std::string DeviceActiveUseCase::GetChromeOSVersion() const {
  return version_info::GetMajorVersionNumber();
}

Channel DeviceActiveUseCase::GetChromeOSChannel() const {
  switch (chrome_passed_device_params_.chromeos_channel) {
    case version_info::Channel::CANARY:
      return Channel::CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return Channel::CHANNEL_DEV;
    case version_info::Channel::BETA:
      return Channel::CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return Channel::CHANNEL_STABLE;
    case version_info::Channel::UNKNOWN:
    default:
      return Channel::CHANNEL_UNKNOWN;
  }
}

MarketSegment DeviceActiveUseCase::GetMarketSegment() const {
  return chrome_passed_device_params_.market_segment;
}

const std::string& DeviceActiveUseCase::GetPsmDeviceActiveSecret() const {
  if (psm_device_active_secret_.empty()) {
    VLOG(1) << "PSM Device Active Secret is not defined.";
  }

  return psm_device_active_secret_;
}

}  // namespace ash::device_activity
