// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ATTESTATION_ATTESTATION_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ATTESTATION_ATTESTATION_CLIENT_H_

#include <deque>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace dbus {
class Bus;
}

namespace ash {

// AttestationClient is used to communicate with the org.chromium.Attestation
// service. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(ASH_DBUS_ATTESTATION) AttestationClient {
 public:
  using GetKeyInfoCallback =
      base::OnceCallback<void(const ::attestation::GetKeyInfoReply&)>;
  using GetEndorsementInfoCallback =
      base::OnceCallback<void(const ::attestation::GetEndorsementInfoReply&)>;
  using GetAttestationKeyInfoCallback = base::OnceCallback<void(
      const ::attestation::GetAttestationKeyInfoReply&)>;
  using ActivateAttestationKeyCallback = base::OnceCallback<void(
      const ::attestation::ActivateAttestationKeyReply&)>;
  using CreateCertifiableKeyCallback =
      base::OnceCallback<void(const ::attestation::CreateCertifiableKeyReply&)>;
  using DecryptCallback =
      base::OnceCallback<void(const ::attestation::DecryptReply&)>;
  using SignCallback =
      base::OnceCallback<void(const ::attestation::SignReply&)>;
  using RegisterKeyWithChapsTokenCallback = base::OnceCallback<void(
      const ::attestation::RegisterKeyWithChapsTokenReply&)>;
  using GetEnrollmentPreparationsCallback = base::OnceCallback<void(
      const ::attestation::GetEnrollmentPreparationsReply&)>;
  using GetFeaturesCallback =
      base::OnceCallback<void(const ::attestation::GetFeaturesReply&)>;
  using GetStatusCallback =
      base::OnceCallback<void(const ::attestation::GetStatusReply&)>;
  using VerifyCallback =
      base::OnceCallback<void(const ::attestation::VerifyReply&)>;
  using CreateEnrollRequestCallback =
      base::OnceCallback<void(const ::attestation::CreateEnrollRequestReply&)>;
  using FinishEnrollCallback =
      base::OnceCallback<void(const ::attestation::FinishEnrollReply&)>;
  using CreateCertificateRequestCallback = base::OnceCallback<void(
      const ::attestation::CreateCertificateRequestReply&)>;
  using FinishCertificateRequestCallback = base::OnceCallback<void(
      const ::attestation::FinishCertificateRequestReply&)>;
  using EnrollCallback =
      base::OnceCallback<void(const ::attestation::EnrollReply&)>;
  using GetCertificateCallback =
      base::OnceCallback<void(const ::attestation::GetCertificateReply&)>;
  using SignEnterpriseChallengeCallback = base::OnceCallback<void(
      const ::attestation::SignEnterpriseChallengeReply&)>;
  using SignSimpleChallengeCallback =
      base::OnceCallback<void(const ::attestation::SignSimpleChallengeReply&)>;
  using SetKeyPayloadCallback =
      base::OnceCallback<void(const ::attestation::SetKeyPayloadReply&)>;
  using DeleteKeysCallback =
      base::OnceCallback<void(const ::attestation::DeleteKeysReply&)>;
  using ResetIdentityCallback =
      base::OnceCallback<void(const ::attestation::ResetIdentityReply&)>;
  using GetEnrollmentIdCallback =
      base::OnceCallback<void(const ::attestation::GetEnrollmentIdReply&)>;
  using GetCertifiedNvIndexCallback =
      base::OnceCallback<void(const ::attestation::GetCertifiedNvIndexReply&)>;

  // Interface with testing functionality. Accessed through GetTestInterface(),
  // only implemented in the fake implementation.
  class TestInterface {
   public:
    // Sets the preparation status to |is_prepared|. If no injected sequence by
    // |ConfigureEnrollmentPreparationsSequence| the enrollment preparations
    // always returns |is_prepared|.
    virtual void ConfigureEnrollmentPreparations(bool is_prepared) = 0;
    // Injects |sequence| of enrollment preparations. Once injected, the
    // returned enrollment preparations status will be the element popped from
    // the |sequence| one-by-one until all the elements are consumed.
    virtual void ConfigureEnrollmentPreparationsSequence(
        std::deque<bool> sequence) = 0;
    // Injects a bad status to `GetEnrollmentPreparations()` calls. By design,
    // this only accepts bad status so |STATUS_SUCCESS| is seen as an illegal
    // input and abort the program. To recover the fake behavior to successful
    // calls, call ConfigureEnrollmentPreparations(Sequence)?.
    virtual void ConfigureEnrollmentPreparationsStatus(
        ::attestation::AttestationStatus status) = 0;

    // Gets the mutable |GetFeaturesReply| that is returned when queried.
    virtual ::attestation::GetFeaturesReply* mutable_features_reply() = 0;

    // Gets the mutable |GetStatusReply| that is returned when queried.
    virtual ::attestation::GetStatusReply* mutable_status_reply() = 0;

    // Allowlists |request| so the certificate requests that comes in afterwards
    // will get a fake certificate. If any alias of |request| has been
    // allowlisted this functions performs no-ops.
    virtual void AllowlistCertificateRequest(
        const ::attestation::GetCertificateRequest& request) = 0;

    // Gets the history of `DeleteKeys()` requests.
    virtual const std::vector<::attestation::DeleteKeysRequest>&
    delete_keys_history() const = 0;

    // Clears the request history of `DeleteKeys()`.
    virtual void ClearDeleteKeysHistory() = 0;

    // Sets returned enrollment ids, when ignoring/not ignoring cache,
    // respectively.
    virtual void set_enrollment_id_ignore_cache(const std::string& id) = 0;
    virtual void set_cached_enrollment_id(const std::string& id) = 0;
    // Sets the returned status of `GetEnrollmentId()` to be D-Bus error for
    // `count` times to emulate D-Bus late availability.
    virtual void set_enrollment_id_dbus_error_count(int count) = 0;

    // Gets the reply to the key info query as a fake database.
    virtual ::attestation::GetKeyInfoReply* GetMutableKeyInfoReply(
        const std::string& username,
        const std::string& label) = 0;
    // Sets the returned status of `GetKeyInfo()` to be D-Bus error for
    // `count` times to emulate D-Bus late availability.
    virtual void set_key_info_dbus_error_count(int count) = 0;
    // Gets the remaining count of `GetKeyInfo()` replying D-Bus error.
    virtual int key_info_dbus_error_count() const = 0;

    // Verifies if `signed_data` is signed against `challenge`.
    virtual bool VerifySimpleChallengeResponse(
        const std::string& challenge,
        const ::attestation::SignedData& signed_data) = 0;

    // Sets the status code returned by `SignSimpleChallenge()`.
    virtual void set_sign_simple_challenge_status(
        ::attestation::AttestationStatus status) = 0;
    // Allowlists the key used to sign simple challenge.
    virtual void AllowlistSignSimpleChallengeKey(const std::string& username,
                                                 const std::string& label) = 0;

    // Sets the status code returned by `Sign()`.
    virtual void set_sign_status(::attestation::AttestationStatus status) = 0;

    // Sets the status code returned by `RegisterKeyWithChapsToken()`.
    virtual void set_register_key_status(
        ::attestation::AttestationStatus status) = 0;
    // Allowlists the key allowed to be registered to the key store.
    virtual void AllowlistRegisterKey(const std::string& username,
                                      const std::string& label) = 0;

    // Sets the status code returned by `SignEnterpriseChallenge()`.
    virtual void set_sign_enterprise_challenge_status(
        ::attestation::AttestationStatus status) = 0;
    // Allowlists the key used to sign enterprise challenge. Note that
    // `include_signed_public_key` and `challenge` are ignored for key
    // comparison because they are are part of the key but the inputs for
    // signature generation.
    virtual void AllowlistSignEnterpriseChallengeKey(
        const ::attestation::SignEnterpriseChallengeRequest& request) = 0;
    // Signs the enterprise `challenge` with this class the same way the
    // enterprise challenge is signed.
    virtual std::string GetEnterpriseChallengeFakeSignature(
        const std::string& challenge,
        bool include_spkac) const = 0;
    // Sets the delay to the time we reply to `SignEnterpriseChallenge()`.
    virtual void set_sign_enterprise_challenge_delay(
        const base::TimeDelta& delay) = 0;

    // Sets the allowed ACA type for the legacy attestation flow. The enroll
    // request can be created/finished only if the ACA type matches the set ACA
    // type. By default, it is default ACA.
    virtual void set_aca_type_for_legacy_flow(
        ::attestation::ACAType aca_type) = 0;

    // Sets the status code returned by `CreateEnrollRequestRequest()`.
    virtual void set_enroll_request_status(
        ::attestation::AttestationStatus status) = 0;
    // Gets the fake enroll request when the status is configured to be good.
    virtual std::string GetFakePcaEnrollRequest() const = 0;
    // Gets the fake enroll response that is accepted by `FinishEnroll()`.
    virtual std::string GetFakePcaEnrollResponse() const = 0;

    // Allowlists the request that has `username`, `request_origin`, `profile`,
    // and `key_type`, so the certificate requests that comes in afterwards will
    // be successfully created.
    virtual void AllowlistLegacyCreateCertificateRequest(
        const std::string& username,
        const std::string& request_origin,
        ::attestation::CertificateProfile profile,
        ::attestation::KeyType key_type) = 0;
    // Sets the status code returned by `CreateCertificateRequest()`.
    virtual void set_cert_request_status(
        ::attestation::AttestationStatus status) = 0;
    // Gets the fake certificate request when the status is configured to be
    // good.
    virtual std::string GetFakePcaCertRequest() const = 0;
    // Gets the fake enroll response that is accepted by
    // `FinishCertificateRequest()`.
    virtual std::string GetFakePcaCertResponse() const = 0;
    // Gets the fake certificate that is returned by
    // successful `FinishCertificateRequest()`.
    virtual std::string GetFakeCertificate() const = 0;
    // Sets the status code returned by `DeleteKeys()`.
    virtual void set_delete_keys_status(
        ::attestation::AttestationStatus status) = 0;
  };

  // Not copyable or movable.
  AttestationClient(const AttestationClient&) = delete;
  AttestationClient& operator=(const AttestationClient&) = delete;
  AttestationClient(AttestationClient&&) = delete;
  AttestationClient& operator=(AttestationClient&&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static AttestationClient* Get();

  // Checks if |reply| indicates the attestation service is prepared with any
  // ACA.
  static bool IsAttestationPrepared(
      const ::attestation::GetEnrollmentPreparationsReply& reply);

  // Gets the verified access server type from command-line arguments.
  static ::attestation::VAType GetVerifiedAccessServerType();

  // Attestation daemon D-Bus method calls. See org.chromium.Attestation.xml and
  // the corresponding protobuf definitions in Chromium OS code for the
  // documentation of the methods and request/ messages.

  virtual void GetKeyInfo(const ::attestation::GetKeyInfoRequest& request,
                          GetKeyInfoCallback callback) = 0;

  virtual void GetEndorsementInfo(
      const ::attestation::GetEndorsementInfoRequest& request,
      GetEndorsementInfoCallback callback) = 0;

  virtual void GetAttestationKeyInfo(
      const ::attestation::GetAttestationKeyInfoRequest& request,
      GetAttestationKeyInfoCallback callback) = 0;

  virtual void ActivateAttestationKey(
      const ::attestation::ActivateAttestationKeyRequest& request,
      ActivateAttestationKeyCallback callback) = 0;

  virtual void CreateCertifiableKey(
      const ::attestation::CreateCertifiableKeyRequest& request,
      CreateCertifiableKeyCallback callback) = 0;

  virtual void Decrypt(const ::attestation::DecryptRequest& request,
                       DecryptCallback callback) = 0;

  virtual void Sign(const ::attestation::SignRequest& request,
                    SignCallback callback) = 0;

  virtual void RegisterKeyWithChapsToken(
      const ::attestation::RegisterKeyWithChapsTokenRequest& request,
      RegisterKeyWithChapsTokenCallback callback) = 0;

  virtual void GetEnrollmentPreparations(
      const ::attestation::GetEnrollmentPreparationsRequest& request,
      GetEnrollmentPreparationsCallback callback) = 0;

  virtual void GetFeatures(const ::attestation::GetFeaturesRequest& request,
                           GetFeaturesCallback callback) = 0;

  virtual void GetStatus(const ::attestation::GetStatusRequest& request,
                         GetStatusCallback callback) = 0;

  virtual void Verify(const ::attestation::VerifyRequest& request,
                      VerifyCallback callback) = 0;

  virtual void CreateEnrollRequest(
      const ::attestation::CreateEnrollRequestRequest& request,
      CreateEnrollRequestCallback callback) = 0;

  virtual void FinishEnroll(const ::attestation::FinishEnrollRequest& request,
                            FinishEnrollCallback callback) = 0;

  virtual void CreateCertificateRequest(
      const ::attestation::CreateCertificateRequestRequest& request,
      CreateCertificateRequestCallback callback) = 0;

  virtual void FinishCertificateRequest(
      const ::attestation::FinishCertificateRequestRequest& request,
      FinishCertificateRequestCallback callback) = 0;

  virtual void Enroll(const ::attestation::EnrollRequest& request,
                      EnrollCallback callback) = 0;

  virtual void GetCertificate(
      const ::attestation::GetCertificateRequest& request,
      GetCertificateCallback callback) = 0;

  virtual void SignEnterpriseChallenge(
      const ::attestation::SignEnterpriseChallengeRequest& request,
      SignEnterpriseChallengeCallback callback) = 0;

  virtual void SignSimpleChallenge(
      const ::attestation::SignSimpleChallengeRequest& request,
      SignSimpleChallengeCallback callback) = 0;

  virtual void SetKeyPayload(const ::attestation::SetKeyPayloadRequest& request,
                             SetKeyPayloadCallback callback) = 0;

  virtual void DeleteKeys(const ::attestation::DeleteKeysRequest& request,
                          DeleteKeysCallback callback) = 0;

  virtual void ResetIdentity(const ::attestation::ResetIdentityRequest& request,
                             ResetIdentityCallback callback) = 0;

  virtual void GetEnrollmentId(
      const ::attestation::GetEnrollmentIdRequest& request,
      GetEnrollmentIdCallback callback) = 0;

  virtual void GetCertifiedNvIndex(
      const ::attestation::GetCertifiedNvIndexRequest& request,
      GetCertifiedNvIndexCallback callback) = 0;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) = 0;

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  AttestationClient();
  virtual ~AttestationClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ATTESTATION_ATTESTATION_CLIENT_H_
