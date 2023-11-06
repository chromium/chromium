// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_ATTESTATION_CERTIFICATE_GENERATOR_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_ATTESTATION_CERTIFICATE_GENERATOR_H_

#include "chromeos/ash/components/phonehub/public/cpp/attestation_certificate_generator.h"

namespace ash::phonehub {

class FakeAttestationCertificateGenerator
    : public AttestationCertificateGenerator {
 public:
  FakeAttestationCertificateGenerator();
  ~FakeAttestationCertificateGenerator() override;

  void RetrieveCertificate() override;

  const std::vector<std::string> CERTS = {"fake_cert"};
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_ATTESTATION_CERTIFICATE_GENERATOR_H_