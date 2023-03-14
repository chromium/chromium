// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ATTESTATION_STUB_ATTESTATION_FEATURES_H_
#define CHROMEOS_ASH_COMPONENTS_ATTESTATION_STUB_ATTESTATION_FEATURES_H_

#include <memory>

#include "chromeos/ash/components/attestation/attestation_features.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"

namespace ash::attestation {

class StubAttestationFeatures : public AttestationFeatures {
 public:
  static std::unique_ptr<StubAttestationFeatures>
  CreateAttestationNotAvailable();
  static std::unique_ptr<StubAttestationFeatures> CreateSupportRsaOnly();
  static std::unique_ptr<StubAttestationFeatures> CreateSupportAll();

  StubAttestationFeatures() = default;
  ~StubAttestationFeatures() override = default;

  StubAttestationFeatures(const StubAttestationFeatures&) = delete;
  StubAttestationFeatures& operator=(const StubAttestationFeatures&) = delete;

  void Init() override {}

  bool IsAttestationAvailable() const override;
  bool IsRsaSupported() const override;
  bool IsEccSupported() const override;

  void Clear();
  void set_is_available(bool is_available);
  void set_is_rsa_supported(bool is_supported);
  void set_is_ecc_supported(bool is_supported);

 private:
  bool is_available_ = false;
  bool is_rsa_supported_ = false;
  bool is_ecc_supported_ = false;
};

class ScopedStubAttestationFeatures {
 public:
  ScopedStubAttestationFeatures();
  explicit ScopedStubAttestationFeatures(
      std::unique_ptr<StubAttestationFeatures> install_attributes);

  ScopedStubAttestationFeatures(const ScopedStubAttestationFeatures&) = delete;
  ScopedStubAttestationFeatures& operator=(
      const ScopedStubAttestationFeatures&) = delete;

  ~ScopedStubAttestationFeatures();

  StubAttestationFeatures* Get();

 private:
  std::unique_ptr<StubAttestationFeatures> attestation_features_;
};

}  // namespace ash::attestation

#endif  // CHROMEOS_ASH_COMPONENTS_ATTESTATION_STUB_ATTESTATION_FEATURES_H_
