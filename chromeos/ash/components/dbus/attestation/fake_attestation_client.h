// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ATTESTATION_FAKE_ATTESTATION_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ATTESTATION_FAKE_ATTESTATION_CLIENT_H_

#include "chromeos/ash/components/dbus/attestation/attestation_client.h"

#include <deque>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "dbus/object_proxy.h"

namespace ash {

class COMPONENT_EXPORT(ASH_DBUS_ATTESTATION) FakeAttestationClient
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
  void GetFeatures(const ::attestation::GetFeaturesRequest& request,
                   GetFeaturesCallback callback) override;
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
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;

  // AttestationClient::TestInterface:
  void ConfigureEnrollmentPreparations(bool is_prepared) override;
  void ConfigureEnrollmentPreparationsSequence(
      std::deque<bool> sequence) override;
  void ConfigureEnrollmentPreparationsStatus(
      ::attestation::AttestationStatus status) override;
  ::attestation::GetFeaturesReply* mutable_features_reply() override;
  ::attestation::GetStatusReply* mutable_status_reply() override;
  void AllowlistCertificateRequest(
      const ::attestation::GetCertificateRequest& request) override;
  const std::vector<::attestation::DeleteKeysRequest>& delete_keys_history()
      const override;
  void ClearDeleteKeysHistory() override;
  void set_enrollment_id_ignore_cache(const std::string& id) override;
  void set_cached_enrollment_id(const std::string& id) override;
  void set_enrollment_id_dbus_error_count(int count) override;
  ::attestation::GetKeyInfoReply* GetMutableKeyInfoReply(
      const std::string& username,
      const std::string& label) override;
  void set_key_info_dbus_error_count(int count) override;
  int key_info_dbus_error_count() const override;
  bool VerifySimpleChallengeResponse(
      const std::string& challenge,
      const ::attestation::SignedData& signed_data) override;
  void set_sign_simple_challenge_status(
      ::attestation::AttestationStatus status) override;
  void AllowlistSignSimpleChallengeKey(const std::string& username,
                                       const std::string& label) override;
  void set_sign_status(::attestation::AttestationStatus status) override;
  void set_register_key_status(
      ::attestation::AttestationStatus status) override;
  void AllowlistRegisterKey(const std::string& username,
                            const std::string& label) override;
  void set_sign_enterprise_challenge_status(
      ::attestation::AttestationStatus status) override;
  void AllowlistSignEnterpriseChallengeKey(
      const ::attestation::SignEnterpriseChallengeRequest& request) override;
  std::string GetEnterpriseChallengeFakeSignature(
      const std::string& challenge,
      bool include_spkac) const override;
  void set_sign_enterprise_challenge_delay(
      const base::TimeDelta& delay) override;
  void set_aca_type_for_legacy_flow(::attestation::ACAType aca_type) override;
  void set_enroll_request_status(
      ::attestation::AttestationStatus status) override;
  std::string GetFakePcaEnrollRequest() const override;
  std::string GetFakePcaEnrollResponse() const override;
  void AllowlistLegacyCreateCertificateRequest(
      const std::string& username,
      const std::string& request_origin,
      ::attestation::CertificateProfile profile,
      ::attestation::KeyType key_type) override;
  void set_cert_request_status(
      ::attestation::AttestationStatus status) override;
  std::string GetFakePcaCertRequest() const override;
  std::string GetFakePcaCertResponse() const override;
  std::string GetFakeCertificate() const override;
  void set_delete_keys_status(::attestation::AttestationStatus status) override;

  AttestationClient::TestInterface* GetTestInterface() override;

 private:
  ::attestation::AttestationStatus preparations_status_ =
      ::attestation::STATUS_SUCCESS;
  bool is_prepared_ = true;
  std::deque<bool> preparation_sequences_;

  ::attestation::GetFeaturesReply features_reply_;
  ::attestation::GetStatusReply status_reply_;

  // Maintains the allowlisted certificate requests.
  std::vector<::attestation::GetCertificateRequest> allowlisted_requests_;

  // Maintains the allowlisted legacy create-certificate requests.
  std::vector<::attestation::CreateCertificateRequestRequest>
      allowlisted_create_requests_;

  // Maintains the numbers assigned to the allowlisted requests.
  std::vector<int> certificate_indices_;
  // The count of certificates that has been issued.
  int certificate_count_ = 0;

  // Maintains the input request history of `DeleteKeys()`.
  std::vector<::attestation::DeleteKeysRequest> delete_keys_history_;

  // Maintains building components reply to `GetEnrollmentId()`.
  std::string enrollment_id_;
  std::string enrollment_id_ignore_cache_;
  int enrollment_id_dbus_error_count_ = 0;

  class GetKeyInfoRequestComparator {
   public:
    bool operator()(const ::attestation::GetKeyInfoRequest& r1,
                    const ::attestation::GetKeyInfoRequest& r2) const {
      return r1.username() == r2.username() ? r1.key_label() < r2.key_label()
                                            : r1.username() < r2.username();
    }
  };
  // The fake key info database. std::map is chosen because we don't have to
  // implement the hash function for the `GetKeyInfoRequest`, which could be
  // expensive and contributes unreasonable overhead at smaller scale, anyway.
  std::map<::attestation::GetKeyInfoRequest,
           ::attestation::GetKeyInfoReply,
           GetKeyInfoRequestComparator>
      key_info_database_;
  int key_info_dbus_error_count_ = 0;

  // The status returned by `SignSimpleChallenge()`.
  ::attestation::AttestationStatus sign_simple_challenge_status_ =
      ::attestation::STATUS_SUCCESS;
  // The status returned by `Sign()`.
  ::attestation::AttestationStatus sign_status_ = ::attestation::STATUS_SUCCESS;
  // The table of username-label pairs of which keys can perform simple sign
  // challenge.
  std::set<std::pair<std::string, std::string>>
      allowlisted_sign_simple_challenge_keys_;

  // The status returned by `RegisterKeyWithChapsToken()`.
  ::attestation::AttestationStatus register_key_status_ =
      ::attestation::STATUS_SUCCESS;
  // The table of username-label pairs of which keys can be registered to the
  // key store.
  std::set<std::pair<std::string, std::string>> allowlisted_register_keys_;

  // The status returned by `SignEnterpriseChallenge()`.
  ::attestation::AttestationStatus sign_enterprise_challenge_status_ =
      ::attestation::STATUS_SUCCESS;

  class SignEnterpriseChallengeRequestComparator {
   public:
    bool operator()(
        const ::attestation::SignEnterpriseChallengeRequest& r1,
        const ::attestation::SignEnterpriseChallengeRequest& r2) const {
      // The inputs for signature generation `challenge()` and
      // `include_signed_public_key()` are ignored.
      return std::forward_as_tuple(r1.username(), r1.key_label(),
                                   r1.key_name_for_spkac(), r1.domain(),
                                   r1.device_id(), r1.va_type(),
                                   r1.include_customer_id(), r1.flow_type(),
                                   r1.include_certificate()) <
             std::forward_as_tuple(r2.username(), r2.key_label(),
                                   r2.key_name_for_spkac(), r2.domain(),
                                   r2.device_id(), r2.va_type(),
                                   r2.include_customer_id(), r2.flow_type(),
                                   r2.include_certificate());
    }
  };
  // The table of `SignEnterpriseChallenge` which can sign enterprise
  // challenge.
  std::set<::attestation::SignEnterpriseChallengeRequest,
           SignEnterpriseChallengeRequestComparator>
      allowlisted_sign_enterprise_challenge_keys_;
  // The delay the reply of `SignEnterpriseChallenge()` is posted with.
  base::TimeDelta sign_enterprise_challenge_delay_;

  // The allowed ACA type for legacy attestation flow.
  ::attestation::ACAType aca_type_for_legacy_mode_ = ::attestation::DEFAULT_ACA;

  // The status returned by `CreateEnrollRequest()`.
  ::attestation::AttestationStatus enroll_request_status_ =
      ::attestation::STATUS_SUCCESS;
  // The status returned by `CreateCertificateRequest()`.
  ::attestation::AttestationStatus cert_request_status_ =
      ::attestation::STATUS_SUCCESS;
  // The status returned by `DeleteKeys()`.
  ::attestation::AttestationStatus delete_keys_status_ =
      ::attestation::STATUS_SUCCESS;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ATTESTATION_FAKE_ATTESTATION_CLIENT_H_
