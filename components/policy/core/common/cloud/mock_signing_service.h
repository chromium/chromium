// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_SIGNING_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_SIGNING_SERVICE_H_

#include "components/policy/core/common/cloud/signing_service.h"

#include "base/functional/callback.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class FakeSigningService : public SigningService {
 public:
  FakeSigningService();
  ~FakeSigningService() override;

  void SignData(const std::string& data, SigningCallback callback) override;

  // Useful for test setups without having to deal with callbacks.
  void SignDataSynchronously(const std::string& data,
      enterprise_management::SignedData* signed_data);

  // Determine whether SignData will appear successful or not.
  void set_success(bool success);

 private:
  bool success_ = true;
};

class MockSigningService : public FakeSigningService {
 public:
  MockSigningService();
  ~MockSigningService() override;

  MOCK_METHOD2(SignRegistrationData,
      void(const enterprise_management::
               CertificateBasedDeviceRegistrationData*,
               enterprise_management::SignedData*));
  MOCK_METHOD2(SignData, void(const std::string&, SigningCallback));
};

}

#endif // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_SIGNING_SERVICE_H_
