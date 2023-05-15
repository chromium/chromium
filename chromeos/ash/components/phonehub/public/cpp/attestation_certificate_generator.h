// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PUBLIC_CPP_ATTESTATION_CERTIFICATE_GENERATOR_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PUBLIC_CPP_ATTESTATION_CERTIFICATE_GENERATOR_H_

#include <vector>
#include "base/functional/callback.h"
namespace ash::phonehub {

// Generates attestation certificates for cross-device communication.
class AttestationCertificateGenerator {
 public:
  AttestationCertificateGenerator() = default;
  virtual ~AttestationCertificateGenerator() = default;
  AttestationCertificateGenerator(const AttestationCertificateGenerator&) =
      delete;
  AttestationCertificateGenerator& operator=(
      const AttestationCertificateGenerator&) = delete;
  using OnCertificateRetrievedCallback =
      base::OnceCallback<void(const std::vector<std::string>& certs,
                              bool valid)>;

  virtual void RetrieveCertificate(OnCertificateRetrievedCallback callback) = 0;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PUBLIC_CPP_ATTESTATION_CERTIFICATE_GENERATOR_H_
