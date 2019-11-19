// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_WAKE_LOCK_ARC_WAKE_LOCK_BRIDGE_H_
#define COMPONENTS_ARC_WAKE_LOCK_ARC_WAKE_LOCK_BRIDGE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/mojom/wake_lock.mojom.h"
#include "components/arc/session/connection_observer.h"
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
                          public mojom::WakeLockHost {
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

  // Runs the message loop until replies have been received for all pending
  // device service requests in |wake_lock_requesters_|.
  void FlushWakeLocksForTesting();

  // mojom::WakeLockHost overrides.
  void AcquirePartialWakeLock(AcquirePartialWakeLockCallback callback) override;
  void ReleasePartialWakeLock(ReleasePartialWakeLockCallback callback) override;

 private:
  class WakeLockRequester;

  // Returns the WakeLockRequester for |type|, creating one if needed.
  WakeLockRequester* GetWakeLockRequester(device::mojom::WakeLockType type);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // If non-null, used instead of the process-wide connector to fetch services.
  service_manager::Connector* connector_for_test_ = nullptr;

  // Used to track Android wake lock requests and acquire and release device
  // service wake locks as needed.
  std::map<device::mojom::WakeLockType, std::unique_ptr<WakeLockRequester>>
      wake_lock_requesters_;

  mojo::Binding<mojom::WakeLockHost> binding_;

  base::WeakPtrFactory<ArcWakeLockBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcWakeLockBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_WAKE_LOCK_ARC_WAKE_LOCK_BRIDGE_H_
