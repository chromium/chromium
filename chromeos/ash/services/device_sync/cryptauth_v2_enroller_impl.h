// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_ENROLLER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_ENROLLER_IMPL_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_creator.h"
#include "chromeos/ash/services/device_sync/cryptauth_v2_enroller.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_enrollment.pb.h"

namespace cryptauthv2 {
class ClientAppMetadata;
class ClientMetadata;
class PolicyReference;
}  // namespace cryptauthv2

namespace ash {

namespace device_sync {

class CryptAuthClient;
class CryptAuthClientFactory;
class CryptAuthKeyRegistry;

// An implementation of CryptAuthV2Enroller, using instances of CryptAuthClient
// to make the API calls to CryptAuth.
class CryptAuthV2EnrollerImpl : public CryptAuthV2Enroller {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthV2Enroller> Create(
        CryptAuthKeyRegistry* key_registry,
        CryptAuthClientFactory* client_factory,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthV2Enroller> CreateInstance(
        CryptAuthKeyRegistry* key_registry,
        CryptAuthClientFactory* client_factory,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_;
  };

  CryptAuthV2EnrollerImpl(const CryptAuthV2EnrollerImpl&) = delete;
  CryptAuthV2EnrollerImpl& operator=(const CryptAuthV2EnrollerImpl&) = delete;

  ~CryptAuthV2EnrollerImpl() override;

 private:
  enum class State {
    kNotStarted,
    kWaitingForSyncKeysResponse,
    kWaitingForKeyCreation,
    kWaitingForEnrollKeysResponse,
    kFinished
  };

  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  static std::optional<base::TimeDelta> GetTimeoutForState(State state);
  static std::optional<CryptAuthEnrollmentResult::ResultCode>
  ResultCodeErrorFromTimeoutDuringState(State state);

  // CryptAuthV2Enroller:
  void OnAttemptStarted(
      const cryptauthv2::ClientMetadata& client_metadata,
      const cryptauthv2::ClientAppMetadata& client_app_metadata,
      const std::optional<cryptauthv2::PolicyReference>&
          client_directive_policy_reference) override;

  // |key_registry|: Holds the key bundles that enrolled with CryptAuth. The
  //     enroller reads the existing keys from the registry and is responsible
  //     for updating the key registry during the enrollment flow.
  // |client_factory|: Creates CryptAuthClient instances for making API calls.
  // |timer|: Handles timeouts for asynchronous operations.
  CryptAuthV2EnrollerImpl(CryptAuthKeyRegistry* key_registry,
                          CryptAuthClientFactory* client_factory,
                          std::unique_ptr<base::OneShotTimer> timer);

  void SetState(State state);
  void OnTimeout();

  // Constructs a SyncKeysRequest with information about every key bundle
  // contained in CryptAuthKeyBundle::AllEnrollableNames().
  cryptauthv2::SyncKeysRequest BuildSyncKeysRequest(
      const cryptauthv2::ClientMetadata& client_metadata,
      const cryptauthv2::ClientAppMetadata& client_app_metadata,
      const std::optional<cryptauthv2::PolicyReference>&
          client_directive_policy_reference);

  cryptauthv2::SyncKeysRequest::SyncSingleKeyRequest BuildSyncSingleKeyRequest(
      const CryptAuthKeyBundle::Name& name,
      const CryptAuthKeyBundle* key_bundle);

  void OnSyncKeysSuccess(const cryptauthv2::SyncKeysResponse& response);

  // Returns null if |sync_keys_response| was processed successfully; otherwise,
  // returns the ResultCode corresponding to the failure. The existing keys in
  // the key registry will be updated according to each list of
  // SyncSingleKeyResponse::key_actions. For each SyncSingleKeyResponse that
  // indicates that a key should be created, values will be added to
  // |new_keys_to_create| and |new_key_directives|.
  std::optional<CryptAuthEnrollmentResult::ResultCode>
  ProcessSingleKeyResponses(
      const cryptauthv2::SyncKeysResponse& sync_keys_response,
      base::flat_map<CryptAuthKeyBundle::Name,
                     CryptAuthKeyCreator::CreateKeyData>* new_keys_to_create,
      base::flat_map<CryptAuthKeyBundle::Name, cryptauthv2::KeyDirective>*
          new_key_directives);

  // A function to help ProcessSingleKeyResponse() handle the key-creation
  // instructions.
  std::optional<CryptAuthEnrollmentResult::ResultCode>
  ProcessKeyCreationInstructions(
      const CryptAuthKeyBundle::Name& bundle_name,
      const cryptauthv2::SyncKeysResponse::SyncSingleKeyResponse&
          single_key_response,
      const std::string& server_ephemeral_dh,
      std::optional<CryptAuthKeyCreator::CreateKeyData>* new_key_to_create,
      std::optional<cryptauthv2::KeyDirective>* new_key_directive);

  void OnSyncKeysFailure(NetworkRequestError error);

  void OnKeysCreated(
      const std::string& session_id,
      const base::flat_map<CryptAuthKeyBundle::Name, cryptauthv2::KeyDirective>&
          new_key_directives,
      const base::flat_map<CryptAuthKeyBundle::Name,
                           std::optional<CryptAuthKey>>& new_keys,
      const std::optional<CryptAuthKey>& client_ephemeral_dh);

  void OnEnrollKeysSuccess(
      const base::flat_map<CryptAuthKeyBundle::Name, cryptauthv2::KeyDirective>&
          new_key_directives,
      const base::flat_map<CryptAuthKeyBundle::Name,
                           std::optional<CryptAuthKey>>& new_keys,
      const cryptauthv2::EnrollKeysResponse& response);

  void OnEnrollKeysFailure(NetworkRequestError error);

  void FinishAttempt(CryptAuthEnrollmentResult::ResultCode result_code);

  raw_ptr<CryptAuthKeyRegistry> key_registry_;

  raw_ptr<CryptAuthClientFactory> client_factory_;

  std::unique_ptr<base::OneShotTimer> timer_;

  State state_ = State::kNotStarted;

  // The time of the last state change. Used for execution time metrics.
  base::TimeTicks last_state_change_timestamp_;

  // The new ClientDirective from SyncKeysResponse. This value is stored in the
  // CryptAuthEnrollmentResult which is passed to the
  // EnrollmentAttemptFinishedCallback. It should be passed as null if a failure
  // occurs before the SyncKeysResponse's |client_directive| field is received
  // or if that field's data is invalid.
  std::optional<cryptauthv2::ClientDirective> new_client_directive_;

  // The order the key handles were sent in each SyncSingleKeyRequests.
  base::flat_map<CryptAuthKeyBundle::Name, std::vector<std::string>>
      key_handle_orders_;

  // The CryptAuthClient for the latest SyncKeysRequest or EnrollKeysRequest.
  // The client can only be used for one call; therefore, for each API call, a
  // new client needs to be generated from |client_factory_|.
  std::unique_ptr<CryptAuthClient> cryptauth_client_;

  // An instance of CryptAuthKeyCreator, used to generate the keys requested in
  // SyncKeysResponse. Information about the newly created keys are sent to
  // CryptAuth in the EnrollKeysRequest.
  std::unique_ptr<CryptAuthKeyCreator> key_creator_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_ENROLLER_IMPL_H_
