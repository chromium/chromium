// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_ACTIVITY_GETTER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_ACTIVITY_GETTER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/services/device_sync/cryptauth_device_activity_getter.h"
#include "chromeos/services/device_sync/cryptauth_gcm_manager.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/services/device_sync/public/cpp/client_app_metadata_provider.h"

namespace chromeos {

namespace device_sync {

class CryptAuthClient;
class CryptAuthClientFactory;

// An implementation of CryptAuthDeviceActivityGetter, using instances of
// CryptAuthClient to make the GetDevicesActivityStatus API calls to CryptAuth.
// This implementation handles timeouts internally, so the callback passed to
// CryptAuthDeviceActivityGetter::GetDevicesActivityStatus() is always
// guaranteed to be invoked.
class CryptAuthDeviceActivityGetterImpl : public CryptAuthDeviceActivityGetter {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthDeviceActivityGetter> Create(
        CryptAuthClientFactory* client_factory,
        ClientAppMetadataProvider* client_app_metadata_provider,
        CryptAuthGCMManager* gcm_manager,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthDeviceActivityGetter> BuildInstance(
        CryptAuthClientFactory* client_factory,
        ClientAppMetadataProvider* client_app_metadata_provider,
        CryptAuthGCMManager* gcm_manager,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_;
  };

  ~CryptAuthDeviceActivityGetterImpl() override;

 private:
  enum class State {
    kNotStarted,
    kWaitingForClientAppMetadata,
    kWaitingForGetDevicesActivityStatusResponse,
    kFinished
  };

  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  CryptAuthDeviceActivityGetterImpl(
      CryptAuthClientFactory* client_factory,
      ClientAppMetadataProvider* client_app_metadata_provider,
      CryptAuthGCMManager* gcm_manager,
      std::unique_ptr<base::OneShotTimer> timer);

  // CryptAuthDeviceActivityGetter:
  void OnAttemptStarted() override;
  void OnClientAppMetadataFetched(
      const base::Optional<cryptauthv2::ClientAppMetadata>&
          client_app_metadata);

  static base::Optional<base::TimeDelta> GetTimeoutForState(State state);
  void SetState(State state);
  void OnTimeout();

  void OnGetDevicesActivityStatusSuccess(
      const cryptauthv2::GetDevicesActivityStatusResponse& response);
  void OnGetDevicesActivityStatusFailure(NetworkRequestError error);

  void OnAttemptError(NetworkRequestError error);

  // The CryptAuthClient for the latest CryptAuth request. The client can only
  // be used for one call; therefore, for each API call, a new client needs to
  // be generated from |client_factory_|.
  std::unique_ptr<CryptAuthClient> cryptauth_client_;

  State state_ = State::kNotStarted;
  CryptAuthClientFactory* client_factory_ = nullptr;
  ClientAppMetadataProvider* client_app_metadata_provider_ = nullptr;
  CryptAuthGCMManager* gcm_manager_ = nullptr;
  std::unique_ptr<base::OneShotTimer> timer_;
  base::WeakPtrFactory<CryptAuthDeviceActivityGetterImpl>
      callback_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CryptAuthDeviceActivityGetterImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_ACTIVITY_GETTER_IMPL_H_
