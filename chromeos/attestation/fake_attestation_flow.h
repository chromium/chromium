// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ATTESTATION_FAKE_ATTESTATION_FLOW_H_
#define CHROMEOS_ATTESTATION_FAKE_ATTESTATION_FLOW_H_

#include <string>

#include "chromeos/attestation/attestation_flow.h"

class AccountId;

namespace chromeos {
namespace attestation {

// This fake class always returns a fake certificate.
class FakeAttestationFlow : public AttestationFlow {
 public:
  FakeAttestationFlow();
  ~FakeAttestationFlow() override;

  void GetCertificate(AttestationCertificateProfile certificate_profile,
                      const AccountId& account_id,
                      const std::string& request_origin,
                      bool force_new_key,
                      const std::string& key_name,
                      CertificateCallback callback) override;
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROMEOS_ATTESTATION_FAKE_ATTESTATION_FLOW_H_
