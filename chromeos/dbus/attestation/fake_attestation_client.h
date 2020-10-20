// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ATTESTATION_FAKE_ATTESTATION_CLIENT_H_
#define CHROMEOS_DBUS_ATTESTATION_FAKE_ATTESTATION_CLIENT_H_

#include "chromeos/dbus/attestation/attestation_client.h"

#include <deque>
#include <vector>

#include "base/component_export.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS_ATTESTATION) FakeAttestationClient
    : public AttestationClient,
      public AttestationClient::TestInterface {
 public:
  FakeAttestationClient();
  ~FakeAttestationClient() override;

  // Not copyable or movable.
  FakeAttestationClient(const FakeAttestationClient&) = delete;
  FakeAttestationClient& operator=(const FakeAttestationClient&) = delete;
  FakeAttestationClient(FakeAttestationClient&&) = delete;
  FakeAttestationClient& operator=(FakeAttestationClient&&) = delete;

  // AttestationClient:
  void GetKeyInfo(const ::attestation::GetKeyInfoRequest& request,
                  GetKeyInfoCallback callback) override;
  void GetEndorsementInfo(
      const ::attestation::GetEndorsementInfoRequest& request,
      GetEndorsementInfoCallback callback) override;
  void GetAttestationKeyInfo(
      const ::attestation::GetAttestationKeyInfoRequest& request,
      GetAttestationKeyInfoCallback callback) override;
  void ActivateAttestationKey(
      const ::attestation::ActivateAttestationKeyRequest& request,
      ActivateAttestationKeyCallback callback) override;
  void CreateCertifiableKey(
      const ::attestation::CreateCertifiableKeyRequest& request,
      CreateCertifiableKeyCallback callback) override;
  void Decrypt(const ::attestation::DecryptRequest& request,
               DecryptCallback callback) override;
  void Sign(const ::attestation::SignRequest& request,
            SignCallback callback) override;
  void RegisterKeyWithChapsToken(
      const ::attestation::RegisterKeyWithChapsTokenRequest& request,
      RegisterKeyWithChapsTokenCallback callback) override;
  void GetEnrollmentPreparations(
      const ::attestation::GetEnrollmentPreparationsRequest& request,
      GetEnrollmentPreparationsCallback callback) override;
  void GetStatus(const ::attestation::GetStatusRequest& request,
                 GetStatusCallback callback) override;
  void Verify(const ::attestation::VerifyRequest& request,
              VerifyCallback callback) override;
  void CreateEnrollRequest(
      const ::attestation::CreateEnrollRequestRequest& request,
      CreateEnrollRequestCallback callback) override;
  void FinishEnroll(const ::attestation::FinishEnrollRequest& request,
                    FinishEnrollCallback callback) override;
  void CreateCertificateRequest(
      const ::attestation::CreateCertificateRequestRequest& request,
      CreateCertificateRequestCallback callback) override;
  void FinishCertificateRequest(
      const ::attestation::FinishCertificateRequestRequest& request,
      FinishCertificateRequestCallback callback) override;
  void Enroll(const ::attestation::EnrollRequest& request,
              EnrollCallback callback) override;
  void GetCertificate(const ::attestation::GetCertificateRequest& request,
                      GetCertificateCallback callback) override;
  void SignEnterpriseChallenge(
      const ::attestation::SignEnterpriseChallengeRequest& request,
      SignEnterpriseChallengeCallback callback) override;
  void SignSimpleChallenge(
      const ::attestation::SignSimpleChallengeRequest& request,
      SignSimpleChallengeCallback callback) override;
  void SetKeyPayload(const ::attestation::SetKeyPayloadRequest& request,
                     SetKeyPayloadCallback callback) override;
  void DeleteKeys(const ::attestation::DeleteKeysRequest& request,
                  DeleteKeysCallback callback) override;
  void ResetIdentity(const ::attestation::ResetIdentityRequest& request,
                     ResetIdentityCallback callback) override;
  void GetEnrollmentId(const ::attestation::GetEnrollmentIdRequest& request,
                       GetEnrollmentIdCallback callback) override;
  void GetCertifiedNvIndex(
      const ::attestation::GetCertifiedNvIndexRequest& request,
      GetCertifiedNvIndexCallback callback) override;

  // AttestationClient::TestInterface:
  void ConfigureEnrollmentPreparations(bool is_prepared) override;
  void ConfigureEnrollmentPreparationsSequence(
      std::deque<bool> sequence) override;
  void ConfigureEnrollmentPreparationsStatus(
      ::attestation::AttestationStatus status) override;
  void AllowlistCertificateRequest(
      const ::attestation::GetCertificateRequest& request) override;

  AttestationClient::TestInterface* GetTestInterface() override;

 private:
  ::attestation::AttestationStatus preparations_status_ =
      ::attestation::STATUS_SUCCESS;
  bool is_prepared_ = true;
  std::deque<bool> preparation_sequences_;

  bool is_enrolled_ = false;

  // Maintains the allowlisted certificate requests.
  std::vector<::attestation::GetCertificateRequest> allowlisted_requests_;
  // Maintains the numbers assigned to the allowlisted requests.
  std::vector<int> certificate_indices_;
  // The count of certificates that has been issued.
  int certificate_count_ = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ATTESTATION_FAKE_ATTESTATION_CLIENT_H_
