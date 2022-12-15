// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/first_active_use_case_impl.h"

#include "ash/constants/ash_features.h"
#include "base/strings/string_number_conversions.h"
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

std::string GetAESNonce(int nonce_length) {
  return std::string(nonce_length, 0);
}

}  // namespace

FirstActiveUseCaseImpl::FirstActiveUseCaseImpl(
    const std::string& psm_device_active_secret,
    const ChromeDeviceMetadataParameters& chrome_passed_device_params,
    PrefService* local_state,
    std::unique_ptr<PsmDelegateInterface> psm_delegate)
    : DeviceActiveUseCase(psm_device_active_secret,
                          chrome_passed_device_params,
                          prefs::kDeviceActiveLastKnownFirstActivePingTimestamp,
                          psm_rlwe::RlweUseCase::CROS_FRESNEL_FIRST_ACTIVE,
                          local_state,
                          std::move(psm_delegate)),
      aead_(crypto::Aead::AES_256_GCM) {
  // Initialize |psm_device_active_secret_bytes|.
  base::HexStringToString(GetPsmDeviceActiveSecret(),
                          &psm_device_active_secret_in_bytes_);

  // Encrypt timestamp string with derived stable secret key.
  aead_.Init(&psm_device_active_secret_in_bytes_);
}

FirstActiveUseCaseImpl::~FirstActiveUseCaseImpl() = default;

std::string FirstActiveUseCaseImpl::GenerateUTCWindowIdentifier(
    base::Time ts) const {
  (void)ts;
  return "FIRST_ACTIVE";
}

bool FirstActiveUseCaseImpl::IsDevicePingRequired(
    base::Time new_ping_ts) const {
  (void)new_ping_ts;
  return true;
}

bool FirstActiveUseCaseImpl::EncryptPsmValueAsCiphertext(base::Time ts) {
  if (IsLastKnownPingTimestampSet())
    ts = GetLastKnownPingTimestamp();

  // Explode and return as UTC time.
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  std::string ts_string_plaintext = base::StringPrintf(
      "%04d-%02d-%02d %02d:%02d:%02d.%03d UTC", exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second,
      exploded.millisecond);

  if (!aead_.Seal(ts_string_plaintext, GetAESNonce(aead_.NonceLength()),
                  std::string() /* additional_data */, &ts_ciphertext_)) {
    VLOG(1) << "AES failed to encrypt timestamp plaintext.";
    return false;
  }

  return true;
}

base::Time FirstActiveUseCaseImpl::DecryptPsmValueAsTimestamp(
    std::string ciphertext) const {
  if (IsLastKnownPingTimestampSet())
    return GetLastKnownPingTimestamp();

  std::string ts_string_decrypted;

  aead_.Open(ciphertext, GetAESNonce(aead_.NonceLength()),
             std::string() /* additional_data */, &ts_string_decrypted);

  base::Time retrieved_ts;

  if (!base::Time::FromUTCString(ts_string_decrypted.c_str(), &retrieved_ts)) {
    VLOG(1) << "Failed to decrypt PSM timestamp from retrieved value.";
    return base::Time::UnixEpoch();
  }

  return retrieved_ts;
}

FresnelImportDataRequest FirstActiveUseCaseImpl::GenerateImportRequestBody() {
  std::string psm_id_str = GetPsmIdentifier().value().sensitive_id();

  // Generate Fresnel PSM import request body.
  FresnelImportDataRequest import_request;

  // Create fresh |DeviceMetadata| object.
  // Note every dimension added to this proto must be approved by privacy.
  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  device_metadata->set_chromeos_version(GetChromeOSVersion());
  device_metadata->set_chromeos_channel(GetChromeOSChannel());

  if (base::FeatureList::IsEnabled(
          features::kDeviceActiveClientFirstActiveCheckMembership)) {
    device_metadata->set_hardware_id(GetFullHardwareClass());
    device_metadata->set_market_segment(GetMarketSegment());
  }

  import_request.set_use_case(GetPsmUseCase());

  import_request.set_plaintext_identifier(psm_id_str);

  // TODO(hirthanan): Store the first active timestamp as an encrypted value in
  // PSM.
  import_request.set_value(ts_ciphertext_);

  return import_request;
}

std::string FirstActiveUseCaseImpl::GetTsCiphertext() const {
  return ts_ciphertext_;
}

}  // namespace ash::device_activity
