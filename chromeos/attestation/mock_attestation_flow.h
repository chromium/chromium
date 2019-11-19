// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ATTESTATION_MOCK_ATTESTATION_FLOW_H_
#define CHROMEOS_ATTESTATION_MOCK_ATTESTATION_FLOW_H_

#include "chromeos/attestation/attestation_flow.h"

#include "base/callback.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"

class AccountId;

namespace chromeos {
namespace attestation {

// A fake server proxy which just appends "_response" to every request.
class FakeServerProxy : public ServerProxy {
 public:
  FakeServerProxy();
  ~FakeServerProxy() override;

  void set_result(bool result) {
    result_ = result;
  }

  void SendEnrollRequest(const std::string& request,
                         const DataCallback& callback) override;

  void SendCertificateRequest(const std::string& request,
                              const DataCallback& callback) override;

 private:
  bool result_;

  DISALLOW_COPY_AND_ASSIGN(FakeServerProxy);
};

class MockServerProxy : public ServerProxy {
 public:
  MockServerProxy();
  virtual ~MockServerProxy();

  void DeferToFake(bool result);
  MOCK_METHOD2(SendEnrollRequest,
               void(const std::string&, const DataCallback&));
  MOCK_METHOD2(SendCertificateRequest,
               void(const std::string&, const DataCallback&));
  MOCK_METHOD0(GetType, PrivacyCAType());

 private:
  FakeServerProxy fake_;
};

// This class can be used to mock AttestationFlow callbacks.
class MockObserver {
 public:
  MockObserver();
  virtual ~MockObserver();

  MOCK_METHOD2(MockCertificateCallback,
               void(AttestationStatus, const std::string&));
};

class MockAttestationFlow : public AttestationFlow {
 public:
  MockAttestationFlow();
  virtual ~MockAttestationFlow();

  MOCK_METHOD6(GetCertificate,
               void(AttestationCertificateProfile,
                    const AccountId& account_id,
                    const std::string&,
                    bool,
                    const std::string&, /* key_name */
                    const CertificateCallback&));
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROMEOS_ATTESTATION_MOCK_ATTESTATION_FLOW_H_
