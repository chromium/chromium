// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_attestation_certificate_generator.h"
#include "chromeos/ash/components/phonehub/public/cpp/attestation_certificate_generator.h"

namespace ash::phonehub {

FakeAttestationCertificateGenerator::FakeAttestationCertificateGenerator() =
    default;

FakeAttestationCertificateGenerator::~FakeAttestationCertificateGenerator() =
    default;

void FakeAttestationCertificateGenerator::RetrieveCertificate() {
  NotifyCertificateGenerated(CERTS, true);
}

}  // namespace ash::phonehub