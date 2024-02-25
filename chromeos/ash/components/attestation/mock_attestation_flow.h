// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ATTESTATION_MOCK_ATTESTATION_FLOW_H_
#define CHROMEOS_ASH_COMPONENTS_ATTESTATION_MOCK_ATTESTATION_FLOW_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "testing/gmock/include/gmock/gmock.h"

class AccountId;

namespace ash {
namespace attestation {

// A fake server proxy which just appends "_response" to every request if no
// response specified.
class FakeServerProxy : public ServerProxy {
 public:
  FakeServerProxy();

  FakeServerProxy(const FakeServerProxy&) = delete;
  FakeServerProxy& operator=(const FakeServerProxy&) = delete;

  ~FakeServerProxy() override;

  void set_result(bool result) { result_ = result; }

  void SendEnrollRequest(const std::string& request,
                         DataCallback callback) override;

  void SendCertificateRequest(const std::string& request,
                              DataCallback callback) override;

  void CheckIfAnyProxyPresent(ProxyPresenceCallback callback) override;

  void set_enroll_response(const std::string& response) {
    enroll_response_ = response;
  }

  void set_cert_response(const std::string& response) {
    cert_response_ = response;
  }

 private:
  bool result_;

  std::string enroll_response_;
  std::string cert_response_;
};

class MockServerProxy : public FakeServerProxy {
 public:
  MockServerProxy();
  ~MockServerProxy() override;

  void DeferToFake(bool result);
  MOCK_METHOD2(SendEnrollRequest, void(const std::string&, DataCallback));
  MOCK_METHOD2(SendCertificateRequest, void(const std::string&, DataCallback));
  MOCK_METHOD0(GetType, PrivacyCAType());
  MOCK_METHOD1(CheckIfAnyProxyPresent, void(ProxyPresenceCallback));

  FakeServerProxy* fake() { return &fake_; }

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
  ~MockAttestationFlow() override;

  MOCK_METHOD(
      void,
      GetCertificate,
      (AttestationCertificateProfile /*certificate_profile*/,
       const AccountId& /*account_id*/,
       const std::string& /*request_origin*/,
       bool /*force_new_key*/,
       ::attestation::KeyType /*key_crypto_type*/,
       const std::string& /*key_name*/,
       const std::optional<
           AttestationFlow::CertProfileSpecificData>& /*profile_specific_data*/,
       CertificateCallback /*callback*/));
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ATTESTATION_MOCK_ATTESTATION_FLOW_H_
