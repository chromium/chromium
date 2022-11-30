// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FIRST_ACTIVE_USE_CASE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FIRST_ACTIVE_USE_CASE_IMPL_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"
#include "crypto/aead.h"

class PrefService;

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash::device_activity {

// Forward declaration from fresnel_service.proto.
class FresnelImportDataRequest;

// Contains the methods required to report the first active use case.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    FirstActiveUseCaseImpl : public DeviceActiveUseCase {
 public:
  FirstActiveUseCaseImpl(
      const std::string& psm_device_active_secret,
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      PrefService* local_state,
      std::unique_ptr<PsmDelegateInterface> psm_delegate);
  FirstActiveUseCaseImpl(const FirstActiveUseCaseImpl&) = delete;
  FirstActiveUseCaseImpl& operator=(const FirstActiveUseCaseImpl&) = delete;
  ~FirstActiveUseCaseImpl() override;

  // DeviceActiveUseCase:
  std::string GenerateUTCWindowIdentifier(base::Time ts) const override;

  // DeviceActiveUseCase:
  bool IsDevicePingRequired(base::Time new_ping_ts) const override;

  // DeviceActiveUseCase:
  bool EncryptPsmValueAsCiphertext(base::Time ts) override;

  // DeviceActiveUseCase:
  base::Time DecryptPsmValueAsTimestamp(std::string ciphertext) const override;

  // DeviceActiveUseCase:
  FresnelImportDataRequest GenerateImportRequestBody() override;

  // For testing:
  std::string GetTsCiphertext() const;

 private:
  // AES encryption mode used to encrypt/decrypt first active timestamp.
  // crypto::Aead aead_(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  crypto::Aead aead_;

  // For AES encryption, we must use a 32 byte key. We can use the byte encoded
  // psm device active secret key, since it's 256 bits == 64 byte hex == 32
  // byte string.
  //
  // This field must outlive |aead_| object as it passes a reference to
  // this variable.
  std::string psm_device_active_secret_in_bytes_;

  // AES-256 encrypted timestamp using the |psm_device_active_secret_in_bytes_|.
  // |ciphertext_| is set when an import request is being created for the first
  // active use case. It is sent with the import request body.
  std::string ts_ciphertext_;
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FIRST_ACTIVE_USE_CASE_IMPL_H_
