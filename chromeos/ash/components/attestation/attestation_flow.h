// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_H_
#define CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class AccountId;

namespace ash {

class AttestationClient;

namespace attestation {

// Interface for access to the Privacy CA server.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ATTESTATION) ServerProxy {
 public:
  using DataCallback =
      base::OnceCallback<void(bool success, const std::string& data)>;
  using ProxyPresenceCallback =
      base::OnceCallback<void(bool is_any_proxy_present)>;
  virtual ~ServerProxy();
  virtual void SendEnrollRequest(const std::string& request,
                                 DataCallback on_response) = 0;
  virtual void SendCertificateRequest(const std::string& request,
                                      DataCallback on_response) = 0;
  virtual PrivacyCAType GetType();

  // Looks ahead and checks if `SendEnrollRequest()` or
  // `SendCertificateRequest()` uses any server proxy for real. Note that the
  // callback only returns a boolean value; in case of any error, it is assumed
  // to have proxies. This decision is motivated by the only caller,
  // `AttestationFlowAdaptive`, has to assume the presence of the proxy if the
  // information is not available.
  virtual void CheckIfAnyProxyPresent(ProxyPresenceCallback callback) = 0;
};

// The interface of the message flow for Chrome OS attestation tasks.
// Generally this consists of coordinating messages between the Chrome OS
// attestation service and the Chrome OS Privacy CA server.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ATTESTATION) AttestationFlow {
 public:
  using CertificateCallback =
      base::OnceCallback<void(AttestationStatus status,
                              const std::string& pem_certificate_chain)>;

  // Certificate profile specific request data. Loosely corresponds to `oneof`
  // the proto fields at `GetCertificateRequest::metadata` in
  // `third_party/cros_system_api/dbus/attestation/interface.proto`.
  // `CertProfileSpecificData` itself is equivalent to a type-safe tagged union
  // type that can represent any of the types inside the `absl::variant`.
  using CertProfileSpecificData =
      absl::variant<::attestation::DeviceSetupCertificateRequestMetadata>;

  // Returns the attestation key type for a given `certificate_profile`.
  //
  // Parameters
  //   certificate_profile - Specifies what kind of certificate the key is for.
  static AttestationKeyType GetKeyTypeForProfile(
      AttestationCertificateProfile certificate_profile);

  virtual ~AttestationFlow() = default;

  // Gets an attestation certificate for a hardware-protected key.  If a key for
  // the given profile does not exist, it will be generated and a certificate
  // request will be made to the Chrome OS Privacy CA to issue a certificate for
  // the key.  If the key already exists and `force_new_key` is false, the
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
  //   key_crypto_type - The crypto type of the key.
  //   key_name - The name of the key. May not be empty.
  //   profile_specific_data - Optional certificate profile specific data. The
  //                           type must correspond to `certificate_profile`.
  //   callback - A callback which will be called when the operation completes.
  //              On success `result` will be true and `data` will contain the
  //              PCA-issued certificate chain in PEM format.
  virtual void GetCertificate(
      AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      bool force_new_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      const std::optional<CertProfileSpecificData>& profile_specific_data,
      CertificateCallback callback) = 0;

 protected:
  enum class EnrollState {
    // Attestation is not available on this device.
    kNotAvailable,

    // Attestation enrollment failed.
    kError,

    // Attestation is enrolled.
    kEnrolled,
  };

  using EnrollCallback = base::OnceCallback<void(EnrollState)>;
};

// Implements the message flow for Chrome OS attestation tasks.  Generally this
// consists of coordinating messages between the Chrome OS attestation service
// and the Chrome OS Privacy CA server.  Sample usage:
//
//    AttestationFlowLegacy flow(std::move(my_server_proxy));
//    AttestationFlowLegacy::CertificateCallback callback =
//        base::BindOnce(&MyCallback);
//    flow.GetCertificate(ENTERPRISE_USER_CERTIFICATE, false, callback);
//
// This class is not thread safe.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ATTESTATION)
    AttestationFlowLegacy : public AttestationFlow {
 public:
  using CertificateCallback =
      base::OnceCallback<void(AttestationStatus status,
                              const std::string& pem_certificate_chain)>;

  // Certificate profile specific request data. Loosely corresponds to `oneof`
  // the proto fields at `GetCertificateRequest::metadata` in
  // `third_party/cros_system_api/dbus/attestation/interface.proto`.
  // `CertProfileSpecificData` itself is equivalent to a type-safe tagged union
  // type that can represent any of the types inside the `absl::variant`.
  using CertProfileSpecificData =
      absl::variant<::attestation::DeviceSetupCertificateRequestMetadata>;

  explicit AttestationFlowLegacy(std::unique_ptr<ServerProxy> server_proxy);

  AttestationFlowLegacy(const AttestationFlowLegacy&) = delete;
  AttestationFlowLegacy& operator=(const AttestationFlowLegacy&) = delete;

  ~AttestationFlowLegacy() override;

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

  void GetCertificate(
      AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      bool force_new_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      const std::optional<CertProfileSpecificData>& profile_specific_data,
      CertificateCallback callback) override;

 private:
  // Handles the result of a call to `GetStatus()` for enrollment status.
  // Reports success if enrollment is complete and otherwise starts the process.
  //
  // Parameters
  //   certificate_profile - Specifies what kind of certificate should be
  //                         requested from the CA.
  //   callback - Called with the success or failure of the enrollment.
  //   result - Result of `GetStatus()`, which contains `enrolled` field.
  void OnEnrollmentCheckComplete(
      AttestationCertificateProfile certificate_profile,
      EnrollCallback callback,
      const ::attestation::GetStatusReply& reply);

  // Asynchronously requests attestation features.
  //
  // Parameters
  //   callback - Called with the success or failure of the enrollment.
  void GetFeatures(EnrollCallback callback);

  // Handles the result of a call to `GetFeatures`.
  // If the features indicate attestation is supported, starts the
  // enrollment process.
  //
  // Parameters
  //   callback - Called with the success or failure of the enrollment.
  //   result - Result of `GetStatus()`, which contains `enrolled` field.
  void OnGetFeaturesComplete(EnrollCallback callback,
                             const ::attestation::GetFeaturesReply& reply);

  // Asynchronously waits for attestation to be ready and start enrollment once
  // it is. If attestation is not ready by the time the flow's timeout is
  // reached, fail.
  //
  // Parameters
  //   end_time - Time after which preparation should time out.
  //   callback - Called with the success or failure of the enrollment.
  void WaitForAttestationPrepared(base::TimeTicks end_time,
                                  EnrollCallback callback);

  // Handles the result of a call to GetEnrollmentPreparations. Starts
  // enrollment on success and retries after `retry_delay_` if not.
  //
  // Parameters
  //   end_time - Time after which preparation should time out.
  //   callback - Called with the success or failure of the enrollment.
  //   reply - Reply from the attestation service.
  void OnPreparedCheckComplete(
      base::TimeTicks end_time,
      EnrollCallback callback,
      const ::attestation::GetEnrollmentPreparationsReply& reply);

  // Called when the attestation daemon has finished creating an enrollment
  // request for the Privacy CA.  The request is asynchronously forwarded as-is
  // to the PCA.
  //
  // Parameters
  //   callback - Called with the success or failure of the enrollment.
  //   reply - The reply of `CreateEnrollRequest()`.
  void SendEnrollRequestToPCA(
      EnrollCallback callback,
      const ::attestation::CreateEnrollRequestReply& reply);

  // Called when the Privacy CA responds to an enrollment request.  The response
  // is asynchronously forwarded as-is to the attestation daemon in order to
  // complete the enrollment operation.
  //
  // Parameters
  //   callback - Called with the success or failure of the enrollment.
  //   success - The status of the Privacy CA operation.
  //   data - The response data from the Privacy CA.
  void SendEnrollResponseToDaemon(EnrollCallback callback,
                                  bool success,
                                  const std::string& data);

  // Called when the attestation daemon completes an enrollment operation.  If
  // the operation was successful, the next_task callback is called.
  //
  // Parameters
  //   callback - Called with the success or failure of the enrollment.
  //   reply - The reply of `FinishEnroll()`.
  void OnEnrollComplete(EnrollCallback callback,
                        const ::attestation::FinishEnrollReply& reply);

  // Asynchronously initiates the certificate request flow.  Attestation
  // enrollment must complete successfully before this operation can succeed.
  //
  // Parameters
  //   certificate_profile - Specifies what kind of certificate should be
  //                         requested from the CA.
  //   account_id - Identifies the active user.
  //   request_origin - An identifier for the origin of this request.
  //   generate_new_key - If set to true a new key is generated.
  //   key_crypto_type - The crypto type of the key.
  //   key_name - The name of the key. If left empty, a default name will be
  //              generated.
  //   profile_specific_data - Optional certificate profile specific data. The
  //                           type must correspond to `certificate_profile`.
  //   callback - Called when the operation completes.
  //   enrolled - Success or failure of the enrollment phase.
  void StartCertificateRequest(
      const AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      bool generate_new_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      const std::optional<CertProfileSpecificData>& profile_specific_data,
      CertificateCallback callback,
      EnrollState enroll_state);

  // Called with the reply to `GetKeyInfo()`. Will query the existing
  // certificate if it exists and otherwise start a new certificate request.
  //
  // Parameters
  //   certificate_profile - Specifies what kind of certificate should be
  //                         requested from the CA.
  //   account_id - Identifies the active user.
  //   request_origin - An identifier for the origin of this request.
  //   key_crypto_type - The crypto type of the key.
  //   key_name - The name of the key. If left empty, a default name will be
  //              generated.
  //   profile_specific_data - Optional certificate profile specific data. The
  //                           type must correspond to `certificate_profile`.
  //   callback - Called when the operation completes.
  //   reply - The reply of `GetKeyInfo()`.
  void OnGetKeyInfoComplete(
      AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      AttestationKeyType key_type,
      const std::optional<CertProfileSpecificData>& profile_specific_data,
      CertificateCallback callback,
      const ::attestation::GetKeyInfoReply& reply);

  // Called when the attestation daemon has finished creating a certificate
  // request for the Privacy CA.  The request is asynchronously forwarded as-is
  // to the PCA.
  //
  // Parameters
  //   key_type - The type of the key for which a certificate is requested.
  //   account_id - Identifies the active user.
  //   key_name - The name of the key for which a certificate is requested.
  //   callback - Called when the operation completes.
  //   reply - the result returned by `AttestationClient`.
  void SendCertificateRequestToPCA(
      AttestationKeyType key_type,
      const AccountId& account_id,
      const std::string& key_name,
      CertificateCallback callback,
      const ::attestation::CreateCertificateRequestReply& reply);

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

  // Called after attestation service finishes processing of a certificate
  // request.
  //
  // Parameters
  //   callback - Called when the operation completes.
  //   reply - The reply of `FinishCertificateRequest()`.
  void OnCertRequestFinished(
      CertificateCallback callback,
      const ::attestation::FinishCertificateRequestReply& reply);

  raw_ptr<AttestationClient, DanglingUntriaged> attestation_client_;
  std::unique_ptr<ServerProxy> server_proxy_;

  base::TimeDelta ready_timeout_;
  base::TimeDelta retry_delay_;

  base::WeakPtrFactory<AttestationFlowLegacy> weak_factory_{this};
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_H_
