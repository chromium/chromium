// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ATTESTATION_ATTESTATION_FLOW_H_
#define CHROMEOS_ATTESTATION_ATTESTATION_FLOW_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class AccountId;

namespace cryptohome {

class AsyncMethodCaller;

}  // namespace cryptohome

namespace chromeos {

class CryptohomeClient;
class AttestationClient;

namespace attestation {

// Interface for access to the Privacy CA server.
class COMPONENT_EXPORT(CHROMEOS_ATTESTATION) ServerProxy {
 public:
  using DataCallback =
      base::OnceCallback<void(bool success, const std::string& data)>;
  virtual ~ServerProxy();
  virtual void SendEnrollRequest(const std::string& request,
                                 DataCallback on_response) = 0;
  virtual void SendCertificateRequest(const std::string& request,
                                      DataCallback on_response) = 0;
  virtual PrivacyCAType GetType();
};

// Implements the message flow for Chrome OS attestation tasks.  Generally this
// consists of coordinating messages between the Chrome OS attestation service
// and the Chrome OS Privacy CA server.  Sample usage:
//
//    AttestationFlow flow(AsyncMethodCaller::GetInstance(),
//                         CryptohomeClient::Get(),
//                         std::move(my_server_proxy));
//    AttestationFlow::CertificateCallback callback = base::Bind(&MyCallback);
//    flow.GetCertificate(ENTERPRISE_USER_CERTIFICATE, false, callback);
//
// This class is not thread safe.
class COMPONENT_EXPORT(CHROMEOS_ATTESTATION) AttestationFlow {
 public:
  using CertificateCallback =
      base::OnceCallback<void(AttestationStatus status,
                              const std::string& pem_certificate_chain)>;

  // Returns the attestation key type for a given |certificate_profile|.
  //
  // Parameters
  //   certificate_profile - Specifies what kind of certificate the key is for.
  static AttestationKeyType GetKeyTypeForProfile(
      AttestationCertificateProfile certificate_profile);

  AttestationFlow(cryptohome::AsyncMethodCaller* async_caller,
                  CryptohomeClient* cryptohome_client,
                  std::unique_ptr<ServerProxy> server_proxy);
  virtual ~AttestationFlow();

  // Sets the timeout for attestation to be ready.
  void set_ready_timeout(base::TimeDelta ready_timeout) {
    ready_timeout_ = ready_timeout;
  }
  // Gets the timeout for attestation to be ready.
  base::TimeDelta ready_timeout() const { return ready_timeout_; }

  // Sets the retry delay.
  void set_retry_delay(base::TimeDelta retry_delay) {
    retry_delay_ = retry_delay;
  }

  // Returns the retry delay.
  base::TimeDelta retry_delay() { return retry_delay_; }

  // Gets an attestation certificate for a hardware-protected key.  If a key for
  // the given profile does not exist, it will be generated and a certificate
  // request will be made to the Chrome OS Privacy CA to issue a certificate for
  // the key.  If the key already exists and |force_new_key| is false, the
  // existing certificate is returned.
  //
  // Parameters
  //   certificate_profile - Specifies what kind of certificate should be
  //                         requested from the CA.
  //   account_id - Identifies the currently active user. This is ignored when
  //                using the enterprise machine cert profile.
  //   request_origin - For content protection profiles, certificate requests
  //                    are origin-specific.  This string must uniquely identify
  //                    the origin of the request.
  //   force_new_key - If set to true, a new key will be generated even if a key
  //                   already exists for the profile.  The new key will replace
  //                   the existing key on success.
  //   key_name - The name of the key. If left empty, a default name derived
  //              from the |certificate_profile| and |account_id| will be used.
  //   callback - A callback which will be called when the operation completes.
  //              On success |result| will be true and |data| will contain the
  //              PCA-issued certificate chain in PEM format.
  virtual void GetCertificate(AttestationCertificateProfile certificate_profile,
                              const AccountId& account_id,
                              const std::string& request_origin,
                              bool force_new_key,
                              const std::string& key_name,
                              CertificateCallback callback);

 private:
  // Handles the result of a call to TpmAttestationIsEnrolled. Reports success
  // if enrollment is complete and otherwise starts the process.
  //
  // Parameters
  //   callback - Called with the success or failure of the enrollment.
  //   result - Result of TpmAttestationIsEnrolled().
  void OnEnrollmentCheckComplete(base::OnceCallback<void(bool)> callback,
                                 base::Optional<bool> result);

  // Asynchronously waits for attestation to be ready and start enrollment once
  // it is. If attestation is not ready by the time the flow's timeout is
  // reached, fail.
  //
  // Parameters
  //   end_time - Time after which preparation should time out.
  //   callback - Called with the success or failure of the enrollment.
  void WaitForAttestationPrepared(base::TimeTicks end_time,
                                  base::OnceCallback<void(bool)> callback);

  // Handles the result of a call to GetEnrollmentPreparations. Starts
  // enrollment on success and retries after |retry_delay_| if not.
  //
  // Parameters
  //   end_time - Time after which preparation should time out.
  //   callback - Called with the success or failure of the enrollment.
  //   reply - Reply from the attestation service.
  void OnPreparedCheckComplete(
      base::TimeTicks end_time,
      base::OnceCallback<void(bool)> callback,
      const ::attestation::GetEnrollmentPreparationsReply& reply);

  // Called when the attestation daemon has finished creating an enrollment
  // request for the Privacy CA.  The request is asynchronously forwarded as-is
  // to the PCA.
  //
  // Parameters
  //   callback - Called with the success or failure of the enrollment.
  //   success - The status of request creation.
  //   data - The request data for the Privacy CA.
  void SendEnrollRequestToPCA(base::OnceCallback<void(bool)> callback,
                              bool success,
                              const std::string& data);

  // Called when the Privacy CA responds to an enrollment request.  The response
  // is asynchronously forwarded as-is to the attestation daemon in order to
  // complete the enrollment operation.
  //
  // Parameters
  //   callback - Called with the success or failure of the enrollment.
  //   success - The status of the Privacy CA operation.
  //   data - The response data from the Privacy CA.
  void SendEnrollResponseToDaemon(base::OnceCallback<void(bool)> callback,
                                  bool success,
                                  const std::string& data);

  // Called when the attestation daemon completes an enrollment operation.  If
  // the operation was successful, the next_task callback is called.
  //
  // Parameters
  //   callback - Called with the success or failure of the enrollment.
  //   success - The status of the enrollment operation.
  //   not_used - An artifact of the cryptohome D-Bus interface; ignored.
  void OnEnrollComplete(base::OnceCallback<void(bool)> callback,
                        bool success,
                        cryptohome::MountError not_used);

  // Asynchronously initiates the certificate request flow.  Attestation
  // enrollment must complete successfully before this operation can succeed.
  //
  // Parameters
  //   certificate_profile - Specifies what kind of certificate should be
  //                         requested from the CA.
  //   account_id - Identifies the active user.
  //   request_origin - An identifier for the origin of this request.
  //   generate_new_key - If set to true a new key is generated.
  //   key_name - The name of the key. If left empty, a default name derived
  //              from the |certificate_profile| and |account_id| will be used.
  //   callback - Called when the operation completes.
  //   enrolled - Success or failure of the enrollment phase.
  void StartCertificateRequest(
      const AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      bool generate_new_key,
      const std::string& key_name,
      CertificateCallback callback,
      bool enrolled);

  // Called with the result of TpmAttestationDoesKeyExist(). Will query the
  // existing certificate if it exists and otherwise start a new certificate
  // request.
  //
  // Parameters
  //   certificate_profile - Specifies what kind of certificate should be
  //                         requested from the CA.
  //   account_id - Identifies the active user.
  //   request_origin - An identifier for the origin of this request.
  //   generate_new_key - If set to true a new key is generated.
  //   key_name - The name of the key. If left empty, a default name derived
  //              from the |certificate_profile| and |account_id| will be used.
  //   callback - Called when the operation completes.
  //   result - Result of TpmAttestationDoesKeyExist().
  void OnKeyExistCheckComplete(
      AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      const std::string& key_name,
      AttestationKeyType key_type,
      CertificateCallback callback,
      base::Optional<bool> result);

  // Called when the attestation daemon has finished creating a certificate
  // request for the Privacy CA.  The request is asynchronously forwarded as-is
  // to the PCA.
  //
  // Parameters
  //   key_type - The type of the key for which a certificate is requested.
  //   account_id - Identifies the active user.
  //   key_name - The name of the key for which a certificate is requested.
  //   callback - Called when the operation completes.
  //   success - The status of request creation.
  //   data - The request data for the Privacy CA.
  void SendCertificateRequestToPCA(AttestationKeyType key_type,
                                   const AccountId& account_id,
                                   const std::string& key_name,
                                   CertificateCallback callback,
                                   bool success,
                                   const std::string& data);

  // Called when the Privacy CA responds to a certificate request.  The response
  // is asynchronously forwarded as-is to the attestation daemon in order to
  // complete the operation.
  //
  // Parameters
  //   key_type - The type of the key for which a certificate is requested.
  //   account_id - Identifies the active user.
  //   key_name - The name of the key for which a certificate is requested.
  //   callback - Called when the operation completes.
  //   success - The status of the Privacy CA operation.
  //   data - The response data from the Privacy CA.
  void SendCertificateResponseToDaemon(AttestationKeyType key_type,
                                       const AccountId& account_id,
                                       const std::string& key_name,
                                       CertificateCallback callback,
                                       bool success,
                                       const std::string& data);

  // Called after cryptohome finishes processing of a certificate request.
  //
  // Parameters
  //   callback - Called when the operation completes.
  //   success - The status of request finishing.
  //   data - The certificate data in PEM format.
  void OnCertRequestFinished(CertificateCallback callback,
                             bool success,
                             const std::string& data);

  // Gets an existing certificate from the attestation daemon.
  //
  // Parameters
  //   key_type - The type of the key for which a certificate is requested.
  //   account_id - Identifies the active user.
  //   key_name - The name of the key for which a certificate is requested.
  //   callback - Called when the operation completes.
  void GetExistingCertificate(AttestationKeyType key_type,
                              const AccountId& account_id,
                              const std::string& key_name,
                              CertificateCallback callback);

  cryptohome::AsyncMethodCaller* async_caller_;
  CryptohomeClient* cryptohome_client_;
  AttestationClient* attestation_client_;
  std::unique_ptr<ServerProxy> server_proxy_;

  base::TimeDelta ready_timeout_;
  base::TimeDelta retry_delay_;

  base::WeakPtrFactory<AttestationFlow> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AttestationFlow);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROMEOS_ATTESTATION_ATTESTATION_FLOW_H_
