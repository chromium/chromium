// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_ACTIVITY_GETTER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_ACTIVITY_GETTER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_activity_getter.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/client_app_metadata_provider.h"

namespace ash {

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
        const std::string& instance_id,
        const std::string& instance_id_token,
        CryptAuthClientFactory* client_factory,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthDeviceActivityGetter> CreateInstance(
        const std::string& instance_id,
        const std::string& instance_id_token,
        CryptAuthClientFactory* client_factory,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_;
  };

  CryptAuthDeviceActivityGetterImpl(const CryptAuthDeviceActivityGetterImpl&) =
      delete;
  CryptAuthDeviceActivityGetterImpl& operator=(
      const CryptAuthDeviceActivityGetterImpl&) = delete;

  ~CryptAuthDeviceActivityGetterImpl() override;

 private:
  enum class State {
    kNotStarted,
    kWaitingForGetDevicesActivityStatusResponse,
    kFinished
  };

  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  CryptAuthDeviceActivityGetterImpl(const std::string& instance_id,
                                    const std::string& instance_id_token,
                                    CryptAuthClientFactory* client_factory,
                                    std::unique_ptr<base::OneShotTimer> timer);

  // CryptAuthDeviceActivityGetter:
  void OnAttemptStarted() override;

  static std::optional<base::TimeDelta> GetTimeoutForState(State state);
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

  // The time of the last state change. Used for execution time metrics.
  base::TimeTicks last_state_change_timestamp_;

  std::string instance_id_;
  std::string instance_id_token_;
  raw_ptr<CryptAuthClientFactory> client_factory_ = nullptr;
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_ACTIVITY_GETTER_IMPL_H_
