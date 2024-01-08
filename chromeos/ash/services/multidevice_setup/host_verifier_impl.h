// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/host_backend_delegate.h"
#include "chromeos/ash/services/multidevice_setup/host_verifier.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace multidevice_setup {

// Concrete HostVerifier implementation, which starts trying to verify a host as
// soon as it is set on the back-end. If verification fails, HostVerifierImpl
// uses an exponential back-off to retry verification until it succeeds.
//
// If the MultiDevice host is changed while verification is in progress, the
// previous verification attempt is canceled and a new attempt begins with the
// updated device.
class HostVerifierImpl : public HostVerifier,
                         public HostBackendDelegate::Observer,
                         public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<HostVerifier> Create(
        HostBackendDelegate* host_backend_delegate,
        device_sync::DeviceSyncClient* device_sync_client,
        PrefService* pref_service,
        base::Clock* clock = base::DefaultClock::GetInstance(),
        std::unique_ptr<base::OneShotTimer> retry_timer =
            std::make_unique<base::OneShotTimer>(),
        std::unique_ptr<base::OneShotTimer> sync_timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<HostVerifier> CreateInstance(
        HostBackendDelegate* host_backend_delegate,
        device_sync::DeviceSyncClient* device_sync_client,
        PrefService* pref_service,
        base::Clock* clock,
        std::unique_ptr<base::OneShotTimer> retry_timer,
        std::unique_ptr<base::OneShotTimer> sync_timer) = 0;

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  HostVerifierImpl(const HostVerifierImpl&) = delete;
  HostVerifierImpl& operator=(const HostVerifierImpl&) = delete;

  ~HostVerifierImpl() override;

 private:
  HostVerifierImpl(HostBackendDelegate* host_backend_delegate,
                   device_sync::DeviceSyncClient* device_sync_client,
                   PrefService* pref_service,
                   base::Clock* clock,
                   std::unique_ptr<base::OneShotTimer> retry_timer,
                   std::unique_ptr<base::OneShotTimer> sync_timer);

  // HostVerifier:
  bool IsHostVerified() override;
  void PerformAttemptVerificationNow() override;

  // HostBackendDelegate::Observer:
  void OnHostChangedOnBackend() override;

  // device_sync::DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  void UpdateRetryState();
  void StopRetryTimerAndClearPrefs();
  void AttemptVerificationWithInitialTimeout();
  void AttemptVerificationAfterInitialTimeout(
      const base::Time& retry_time_from_prefs);
  void StartRetryTimer(const base::Time& time_to_fire);
  void AttemptHostVerification();
  void OnFindEligibleDevicesResult(
      device_sync::mojom::NetworkRequestResult result,
      multidevice::RemoteDeviceRefList eligible_devices,
      multidevice::RemoteDeviceRefList ineligible_devices);
  void OnNotifyDevicesFinished(device_sync::mojom::NetworkRequestResult result);
  void OnSyncTimerFired();

  raw_ptr<HostBackendDelegate> host_backend_delegate_;
  raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<base::Clock> clock_;
  std::unique_ptr<base::OneShotTimer> retry_timer_;
  std::unique_ptr<base::OneShotTimer> sync_timer_;
  base::WeakPtrFactory<HostVerifierImpl> weak_ptr_factory_{this};
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_IMPL_H_
