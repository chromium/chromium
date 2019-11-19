// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_IMPL_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/services/device_sync/cryptauth_ecies_encryptor.h"
#include "chromeos/services/device_sync/cryptauth_group_private_key_sharer.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"

namespace chromeos {

namespace device_sync {

class CryptAuthClient;
class CryptAuthClientFactory;
class CryptAuthKey;

// An implementation of CryptAuthGroupPrivateKeySharer, using instances of
// CryptAuthClient to make the ShareGroupPrivateKey API calls to CryptAuth.
// Timeouts are handled internally, so ShareGroupPrivateKey() is always
// guaranteed to return.
class CryptAuthGroupPrivateKeySharerImpl
    : public CryptAuthGroupPrivateKeySharer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthGroupPrivateKeySharer> BuildInstance(
        CryptAuthClientFactory* client_factory,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

  ~CryptAuthGroupPrivateKeySharerImpl() override;

 private:
  enum class State {
    kNotStarted,
    kWaitingForGroupPrivateKeyEncryption,
    kWaitingForShareGroupPrivateKeyResponse,
    kFinished
  };

  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  static base::Optional<base::TimeDelta> GetTimeoutForState(State state);
  static base::Optional<CryptAuthDeviceSyncResult::ResultCode>
  ResultCodeErrorFromTimeoutDuringState(State state);

  CryptAuthGroupPrivateKeySharerImpl(CryptAuthClientFactory* client_factory,
                                     std::unique_ptr<base::OneShotTimer> timer);

  // CryptAuthGroupPrivateKeySharer:
  void OnAttemptStarted(
      const cryptauthv2::RequestContext& request_context,
      const CryptAuthKey& group_key,
      const IdToEncryptingKeyMap& id_to_encrypting_key_map) override;

  void SetState(State state);
  void OnTimeout();

  void OnGroupPrivateKeysEncrypted(
      const cryptauthv2::RequestContext& request_context,
      const CryptAuthKey& group_key,
      const CryptAuthEciesEncryptor::IdToOutputMap&
          id_to_encrypted_group_private_key_map);
  void OnShareGroupPrivateKeySuccess(
      const cryptauthv2::ShareGroupPrivateKeyResponse& response);
  void OnShareGroupPrivateKeyFailure(NetworkRequestError error);

  void FinishAttempt(CryptAuthDeviceSyncResult::ResultCode result_code);

  // Used for batch encrypting the group private key with each encrypting key.
  std::unique_ptr<CryptAuthEciesEncryptor> encryptor_;

  // Used to make the ShareGroupPrivateKey API call to CryptAuth.
  std::unique_ptr<CryptAuthClient> cryptauth_client_;

  // The time of the last state change. Used for execution time metrics.
  base::TimeTicks last_state_change_timestamp_;

  bool did_non_fatal_error_occur_ = false;
  State state_ = State::kNotStarted;
  CryptAuthClientFactory* client_factory_ = nullptr;
  std::unique_ptr<base::OneShotTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthGroupPrivateKeySharerImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_IMPL_H_
