// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_INTEGRATED_H_
#define CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_INTEGRATED_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class AccountId;

namespace ash {

class AttestationClient;

namespace attestation {

// Implements the message flow for Chrome OS attestation tasks by checking the
// enrollment preparatoins and then send certificate request. This is meant to
// replace its base class, |AttestationFlow|; after all consumptions switch to
// this class the base class will be converted to pure virtual interface or even
// removed.
//
// Note: This class is not thread safe.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ATTESTATION)
    AttestationFlowIntegrated : public AttestationFlow {
 public:
  AttestationFlowIntegrated();
  explicit AttestationFlowIntegrated(::attestation::ACAType aca_type);
  ~AttestationFlowIntegrated() override;

  // Not copyable or movable.
  AttestationFlowIntegrated(const AttestationFlowIntegrated&) = delete;
  AttestationFlowIntegrated(AttestationFlowIntegrated&&) = delete;
  AttestationFlowIntegrated& operator=(const AttestationFlowIntegrated&) =
      delete;
  AttestationFlowIntegrated& operator=(AttestationFlowIntegrated&&) = delete;

  // Sets the timeout for attestation to be ready.
  void set_ready_timeout_for_testing(base::TimeDelta ready_timeout) {
    ready_timeout_ = ready_timeout;
  }

  // Sets the retry delay.
  void set_retry_delay_for_testing(base::TimeDelta retry_delay) {
    retry_delay_ = retry_delay;
  }

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
  //   key_crypto_type - The crypto type of the key.
  //   key_name - The name of the key. May not be empty.
  //   profile_specific_data - Optional certificate profile specific data. The
  //                           type must correspond to `certificate_profile`.
  //   callback - A callback which will be called when the operation completes.
  //              On success |result| will be true and |data| will contain the
  //              PCA-issued certificate chain in PEM format.
  void GetCertificate(
      AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      bool force_new_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      const std::optional<AttestationFlow::CertProfileSpecificData>&
          profile_specific_data,
      CertificateCallback callback) override;

 private:
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

  // Handles the result of a call to TpmAttestationIsPrepared. Starts enrollment
  // on success and retries after |retry_delay_| if not.
  //
  // Parameters
  //   end_time - Time after which preparation should time out.
  //   callback - Called with the success or failure of the enrollment.
  //   reply - Reply from the attestation service.
  void OnPreparedCheckComplete(
      base::TimeTicks end_time,
      EnrollCallback callback,
      const ::attestation::GetEnrollmentPreparationsReply& reply);

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
  //   key_name - The name of the key. If left empty, a default name derived
  //              from the |certificate_profile| and |account_id| will be used.
  //   profile_specific_data - Optional certificate profile specific data. The
  //                           type must correspond to `certificate_profile`.
  //   callback - Called when the operation completes.
  //   is_prepared - Success or failure of the enrollment preparation phase.
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

  // Called after cryptohome finishes processing of a certificate request.
  //
  // Parameters
  //   callback - Called when the operation completes.
  //   reply - The reply from |attestation_client_|.
  void OnCertRequestFinished(CertificateCallback callback,
                             const ::attestation::GetCertificateReply& reply);

  ::attestation::ACAType aca_type_;
  raw_ptr<AttestationClient> attestation_client_;

  base::TimeDelta ready_timeout_;
  base::TimeDelta retry_delay_;

  base::WeakPtrFactory<AttestationFlowIntegrated> weak_factory_{this};
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_ATTESTATION_ATTESTATION_FLOW_INTEGRATED_H_
