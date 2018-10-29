// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_WAKE_LOCK_ARC_WAKE_LOCK_BRIDGE_H_
#define COMPONENTS_ARC_WAKE_LOCK_ARC_WAKE_LOCK_BRIDGE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/arc/common/wake_lock.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/arc/wake_lock/wake_lock_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Sets wake up timers / alarms based on calls from the instance.
class ArcWakeLockBridge : public KeyedService,
                          public ConnectionObserver<mojom::WakeLockInstance>,
                          public mojom::WakeLockHost,
                          public chromeos::PowerManagerClient::Observer,
                          public WakeLockObserver {
 public:
  // Returns the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcWakeLockBridge* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcWakeLockBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcWakeLockBridge(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcWakeLockBridge() override;

  void set_connector_for_testing(service_manager::Connector* connector) {
    connector_for_test_ = connector;
  }

  // ConnectionObserver<mojom::WakeLockInstance>::Observer overrides.
  void OnConnectionClosed() override;

  // mojom::WakeLockHost overrides.
  void AcquirePartialWakeLock(AcquirePartialWakeLockCallback callback) override;
  void ReleasePartialWakeLock(ReleasePartialWakeLockCallback callback) override;

  // chromeos::PowerManagerClient::Observer overrides.
  void DarkSuspendImminent() override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // WakeLockObserver override.
  void OnWakeLockRelease() override;

  // Runs the message loop until replies have been received for all pending
  // device service requests in |wake_lock_requesters_|.
  void FlushWakeLocksForTesting();

  // Checks if |suspend_readiness_cb_| is set.
  bool IsSuspendReadinessStateSetForTesting() const;

  // Returns true iff wake lock of |type| has observers.
  bool WakeLockHasObserversForTesting(device::mojom::WakeLockType type);

  // Time after a dark resume when wake lock count is checked and a decision is
  // made to re-suspend or wait for wake lock release.
  static constexpr base::TimeDelta kDarkResumeWakeLockCheckTimeout =
      base::TimeDelta::FromSeconds(3);

  // Max time to wait for wake lock release after a wake lock check after a dark
  // resume. After this time the system is asked to re-suspend.
  static constexpr base::TimeDelta kDarkResumeHardTimeout =
      base::TimeDelta::FromSeconds(10);

 private:
  class WakeLockRequester;

  // Returns the WakeLockRequester for |type|, creating one if needed.
  WakeLockRequester* GetWakeLockRequester(device::mojom::WakeLockType type);

  // Runs |kDarkResumeWakeLockCheckTimeout| time delta after a dark resume.
  // Checks if app suspension wake locks (partial wake locks for Android) are
  // held. If no wake locks are held then re-suspends the device else schedules
  // |HandleDarkResumeHardTimeout|.
  void HandleDarkResumeWakeLockCheckTimeout();

  // Runs |kDarkResumeHardTimeout| time delta after a
  // |HandleDarkResumeWakeLockCheckTimeout|. Clears all dark resume state and
  // re-suspends the device.
  void HandleDarkResumeHardTimeout();

  // Clears all state associated with dark resume.
  void ClearDarkResumeState();

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // If non-null, used instead of the process-wide connector to fetch services.
  service_manager::Connector* connector_for_test_ = nullptr;

  // Used to track Android wake lock requests and acquire and release device
  // service wake locks as needed.
  std::map<device::mojom::WakeLockType, std::unique_ptr<WakeLockRequester>>
      wake_lock_requesters_;

  // Called when system is ready to supend after a |DarkSupendImminent| i.e.
  // after a dark resume.
  base::OnceClosure suspend_readiness_cb_;

  mojo::Binding<mojom::WakeLockHost> binding_;

  // Used for checking if |DarkResumeWakeLockCheckTimeout| and
  // |DarkResumeHardTimeout| run on the same sequence.
  SEQUENCE_CHECKER(dark_resume_tasks_sequence_checker_);

  // Factory used to schedule and cancel
  // |HandleDarkResumeWakeLockCheckTimeout| and |HandleDarkResumeHardTimeout|.
  // At any point either none or one of these tasks is in flight.
  base::WeakPtrFactory<ArcWakeLockBridge> dark_resume_weak_ptr_factory_{this};

  base::WeakPtrFactory<ArcWakeLockBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcWakeLockBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_WAKE_LOCK_ARC_WAKE_LOCK_BRIDGE_H_
