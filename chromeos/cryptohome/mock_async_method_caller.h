// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_MOCK_ASYNC_METHOD_CALLER_H_
#define CHROMEOS_CRYPTOHOME_MOCK_ASYNC_METHOD_CALLER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cryptohome {

class MockAsyncMethodCaller : public AsyncMethodCaller {
 public:
  static const char kFakeAttestationEnrollRequest[];
  static const char kFakeAttestationCertRequest[];
  static const char kFakeAttestationCert[];
  static const char kFakeSanitizedUsername[];
  static const char kFakeChallengeResponse[];

  MockAsyncMethodCaller();
  virtual ~MockAsyncMethodCaller();

  void SetUp(bool success, MountError return_code);

  MOCK_METHOD2(AsyncTpmAttestationCreateEnrollRequest,
               void(chromeos::attestation::PrivacyCAType pca_type,
                    const DataCallback& callback));
  MOCK_METHOD3(AsyncTpmAttestationEnroll,
               void(chromeos::attestation::PrivacyCAType pca_type,
                    const std::string& pca_response,
                    const Callback& callback));
  MOCK_METHOD5(
      AsyncTpmAttestationCreateCertRequest,
      void(chromeos::attestation::PrivacyCAType pca_type,
           chromeos::attestation::AttestationCertificateProfile profile,
           const Identification& user_id,
           const std::string& request_origin,
           const DataCallback& callback));
  MOCK_METHOD5(AsyncTpmAttestationFinishCertRequest,
               void(const std::string& pca_response,
                    chromeos::attestation::AttestationKeyType key_type,
                    const Identification& user_id,
                    const std::string& key_name,
                    const DataCallback& callback));
  MOCK_METHOD4(TpmAttestationRegisterKey,
               void(chromeos::attestation::AttestationKeyType key_type,
                    const Identification& user_id,
                    const std::string& key_name,
                    const Callback& callback));
  MOCK_METHOD9(TpmAttestationSignEnterpriseChallenge,
               void(chromeos::attestation::AttestationKeyType key_type,
                    const Identification& user_id,
                    const std::string& key_name,
                    const std::string& domain,
                    const std::string& device_id,
                    chromeos::attestation::AttestationChallengeOptions options,
                    const std::string& challenge,
                    const std::string& key_name_for_spkac,
                    const DataCallback& callback));
  MOCK_METHOD5(TpmAttestationSignSimpleChallenge,
               void(chromeos::attestation::AttestationKeyType key_type,
                    const Identification& user_id,
                    const std::string& key_name,
                    const std::string& challenge,
                    const DataCallback& callback));
 private:
  bool success_;
  MountError return_code_;

  void DoCallback(Callback callback);
  // Default fakes for attestation calls.
  void FakeCreateEnrollRequest(const DataCallback& callback);
  void FakeCreateCertRequest(const DataCallback& callback);
  void FakeFinishCertRequest(const DataCallback& callback);
  void FakeGetSanitizedUsername(const DataCallback& callback);
  void FakeEnterpriseChallenge(const DataCallback& callback);

  DISALLOW_COPY_AND_ASSIGN(MockAsyncMethodCaller);
};

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_MOCK_ASYNC_METHOD_CALLER_H_
