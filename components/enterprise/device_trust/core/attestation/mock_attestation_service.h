// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_ATTESTATION_MOCK_ATTESTATION_SERVICE_H_
#define COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_ATTESTATION_MOCK_ATTESTATION_SERVICE_H_

#include <string>

#include "base/values.h"
#include "components/enterprise/device_trust/core/attestation/attestation_service.h"
#include "components/enterprise/device_trust/core/common_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

// Interface for classes in charge of building challenge-responses to enable
// handshake between Chrome, an IdP and Verified Access.
class MockAttestationService : public AttestationService {
 public:
  MockAttestationService();
  ~MockAttestationService() override;

  MOCK_METHOD(void,
              BuildChallengeResponseForVAChallenge,
              (const std::string&,
               base::DictValue,
               const std::set<DTCPolicyLevel>&,
               AttestationCallback),
              (override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_ATTESTATION_MOCK_ATTESTATION_SERVICE_H_
