// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation_certificate_generator.h"

namespace ash::phonehub {

AttestationCertificateGenerator::AttestationCertificateGenerator() = default;
AttestationCertificateGenerator::~AttestationCertificateGenerator() = default;

void AttestationCertificateGenerator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AttestationCertificateGenerator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AttestationCertificateGenerator::NotifyCertificateGenerated(
    const std::vector<std::string>& certs,
    bool is_valid) {
  for (auto& observer : observers_) {
    observer.OnCertificateGenerated(certs, is_valid);
  }
}

}  // namespace ash::phonehub
