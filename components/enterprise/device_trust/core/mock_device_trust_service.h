// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_MOCK_DEVICE_TRUST_SERVICE_H_
#define COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_MOCK_DEVICE_TRUST_SERVICE_H_

#include "components/enterprise/device_trust/core/device_trust_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {
namespace test {

class MockDeviceTrustService : public DeviceTrustService {
 public:
  MockDeviceTrustService();
  ~MockDeviceTrustService() override;

  MOCK_METHOD(bool, IsEnabled, (), (const, override));
  MOCK_METHOD(void,
              BuildChallengeResponse,
              (const std::string&,
               const std::set<DTCPolicyLevel>&,
               DeviceTrustCallback),
              (override));
  MOCK_METHOD(const std::set<DTCPolicyLevel>,
              Watches,
              (const GURL&),
              (const, override));
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_DEVICE_TRUST_CORE_MOCK_DEVICE_TRUST_SERVICE_H_
