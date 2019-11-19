// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_NOTIFIER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_NOTIFIER_IMPL_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/services/device_sync/cryptauth_device_notifier.h"
#include "chromeos/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"

namespace chromeos {

namespace device_sync {

class ClientAppMetadataProvider;
class CryptAuthClient;
class CryptAuthClientFactory;
class CryptAuthGCMManager;

// An implementation of CryptAuthDeviceNotifier, using instances of
// CryptAuthClient to make the BatchNotifyGroupDevices API calls to CryptAuth.
// The requests made via NotifyDevices() are queued and processed sequentially.
// This implementation handles timeouts internally, so a callback passed to
// NotifyDevices() is always guaranteed to be invoked.
class CryptAuthDeviceNotifierImpl : public CryptAuthDeviceNotifier {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthDeviceNotifier> BuildInstance(
        ClientAppMetadataProvider* client_app_metadata_provider,
        CryptAuthClientFactory* client_factory,
        CryptAuthGCMManager* gcm_manager,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

  ~CryptAuthDeviceNotifierImpl() override;

 private:
  enum class State {
    kIdle,
    kWaitingForClientAppMetadata,
    kWaitingForBatchNotifyGroupDevicesResponse
  };

  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  static base::Optional<base::TimeDelta> GetTimeoutForState(State state);

  struct Request {
    Request(const base::flat_set<std::string>& device_ids,
            cryptauthv2::TargetService target_service,
            CryptAuthFeatureType feature_type,
            base::OnceClosure success_callback,
            base::OnceCallback<void(NetworkRequestError)> error_callback);

    Request(Request&& request);

    ~Request();

    base::flat_set<std::string> device_ids;
    cryptauthv2::TargetService target_service;
    CryptAuthFeatureType feature_type;
    base::OnceClosure success_callback;
    base::OnceCallback<void(NetworkRequestError)> error_callback;
  };

  CryptAuthDeviceNotifierImpl(
      ClientAppMetadataProvider* client_app_metadata_provider,
      CryptAuthClientFactory* client_factory,
      CryptAuthGCMManager* gcm_manager,
      std::unique_ptr<base::OneShotTimer> timer);

  // CryptAuthDeviceNotifier:
  void NotifyDevices(
      const base::flat_set<std::string>& device_ids,
      cryptauthv2::TargetService target_service,
      CryptAuthFeatureType feature_type,
      base::OnceClosure success_callback,
      base::OnceCallback<void(NetworkRequestError)> error_callback) override;

  void SetState(State state);
  void OnTimeout();

  void ProcessRequestQueue();
  void OnClientAppMetadataFetched(
      const base::Optional<cryptauthv2::ClientAppMetadata>&
          client_app_metadata);
  void OnBatchNotifyGroupDevicesSuccess(
      const cryptauthv2::BatchNotifyGroupDevicesResponse& response);
  void OnBatchNotifyGroupDevicesFailure(NetworkRequestError error);
  void FinishAttempt(base::Optional<NetworkRequestError> error);

  State state_ = State::kIdle;
  base::TimeTicks last_state_change_timestamp_;
  base::Optional<cryptauthv2::ClientAppMetadata> client_app_metadata_;
  base::queue<Request> pending_requests_;

  ClientAppMetadataProvider* client_app_metadata_provider_ = nullptr;
  CryptAuthClientFactory* client_factory_ = nullptr;
  CryptAuthGCMManager* gcm_manager_ = nullptr;
  std::unique_ptr<CryptAuthClient> cryptauth_client_;
  std::unique_ptr<base::OneShotTimer> timer_;
  base::WeakPtrFactory<CryptAuthDeviceNotifierImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CryptAuthDeviceNotifierImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_NOTIFIER_IMPL_H_
