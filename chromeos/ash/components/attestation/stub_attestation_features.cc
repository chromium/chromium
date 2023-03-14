// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/stub_attestation_features.h"

#include <memory>

#include "base/check_op.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"

namespace ash::attestation {

// static
std::unique_ptr<StubAttestationFeatures>
StubAttestationFeatures::CreateAttestationNotAvailable() {
  auto result = std::make_unique<StubAttestationFeatures>();
  return result;
}

// static
std::unique_ptr<StubAttestationFeatures>
StubAttestationFeatures::CreateSupportRsaOnly() {
  auto result = std::make_unique<StubAttestationFeatures>();
  result->set_is_available(true);
  result->set_is_rsa_supported(true);
  return result;
}

// static
std::unique_ptr<StubAttestationFeatures>
StubAttestationFeatures::CreateSupportAll() {
  auto result = std::make_unique<StubAttestationFeatures>();
  result->set_is_available(true);
  result->set_is_rsa_supported(true);
  result->set_is_ecc_supported(true);
  return result;
}

bool StubAttestationFeatures::IsAttestationAvailable() const {
  return is_available_;
}

bool StubAttestationFeatures::IsRsaSupported() const {
  return is_rsa_supported_;
}

bool StubAttestationFeatures::IsEccSupported() const {
  return is_ecc_supported_;
}

void StubAttestationFeatures::Clear() {
  is_available_ = false;
  is_rsa_supported_ = false;
  is_ecc_supported_ = false;
}

void StubAttestationFeatures::set_is_available(bool is_available) {
  is_available_ = is_available;
}

void StubAttestationFeatures::set_is_rsa_supported(bool is_supported) {
  is_rsa_supported_ = is_supported;
}

void StubAttestationFeatures::set_is_ecc_supported(bool is_supported) {
  is_ecc_supported_ = is_supported;
}

ScopedStubAttestationFeatures::ScopedStubAttestationFeatures()
    : ScopedStubAttestationFeatures(
          StubAttestationFeatures::CreateSupportAll()) {}

ScopedStubAttestationFeatures::ScopedStubAttestationFeatures(
    std::unique_ptr<StubAttestationFeatures> attestation_features)
    : attestation_features_(std::move(attestation_features)) {
  AttestationFeatures::SetForTesting(attestation_features_.get());
}

ScopedStubAttestationFeatures::~ScopedStubAttestationFeatures() {
  CHECK_EQ(attestation_features_.get(), AttestationFeatures::Get());
  AttestationFeatures::ShutdownForTesting();
}

StubAttestationFeatures* ScopedStubAttestationFeatures::Get() {
  return attestation_features_.get();
}

}  // namespace ash::attestation
