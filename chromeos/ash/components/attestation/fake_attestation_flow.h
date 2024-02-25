// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ATTESTATION_FAKE_ATTESTATION_FLOW_H_
#define CHROMEOS_ASH_COMPONENTS_ATTESTATION_FAKE_ATTESTATION_FLOW_H_

#include <optional>
#include <string>

#include "chromeos/ash/components/attestation/attestation_flow.h"

class AccountId;

namespace ash {
namespace attestation {

// This fake class returns either a fake or supplied certificate.
class FakeAttestationFlow : public AttestationFlow {
 public:
  explicit FakeAttestationFlow(const std::string& certificate);
  ~FakeAttestationFlow() override;

  void GetCertificate(
      AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      bool force_new_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      const std::optional<AttestationFlow::CertProfileSpecificData>&
          profile_specific_data,
      CertificateCallback callback) override;

 private:
  std::string certificate_;
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ATTESTATION_FAKE_ATTESTATION_FLOW_H_
