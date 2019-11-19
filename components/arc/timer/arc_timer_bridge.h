// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TIMER_ARC_TIMER_BRIDGE_H_
#define COMPONENTS_ARC_TIMER_ARC_TIMER_BRIDGE_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/arc/mojom/timer.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Sets wake up timers / alarms based on calls from the instance.
class ArcTimerBridge : public KeyedService,
                       public ConnectionObserver<mojom::TimerInstance>,
                       public mojom::TimerHost {
 public:
  using TimerId = int32_t;

  // Returns the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcTimerBridge* GetForBrowserContext(content::BrowserContext* context);

  static ArcTimerBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcTimerBridge(content::BrowserContext* context,
                 ArcBridgeService* bridge_service);
  ~ArcTimerBridge() override;

  // ConnectionObserver<mojom::TimerInstance>::Observer overrides.
  void OnConnectionClosed() override;

  // mojom::TimerHost overrides.
  void CreateTimers(
      std::vector<arc::mojom::CreateTimerRequestPtr> arc_timer_requests,
      CreateTimersCallback callback) override;
  void StartTimer(clockid_t clock_id,
                  base::TimeTicks absolute_expiration_time,
                  StartTimerCallback callback) override;

 private:
  // Deletes all timers.
  void DeleteArcTimers();

  // Callback for (powerd API) call made in |DeleteArcTimers|.
  void OnDeleteArcTimers(bool result);

  // Callback for powerd's D-Bus API called in |CreateTimers|.
  void OnCreateArcTimers(std::vector<clockid_t> clock_ids,
                         CreateTimersCallback callback,
                         base::Optional<std::vector<TimerId>> timer_ids);

  // Retrieves the timer id corresponding to |clock_id|. If a mapping exists in
  // |timer_ids_| then returns an int32_t >= 0. Else returns base::nullopt.
  base::Optional<TimerId> GetTimerId(clockid_t clock_id) const;

  // Owned by ArcServiceManager.
  ArcBridgeService* const arc_bridge_service_;

  // Mapping of clock ids (coresponding to <sys/timerfd.h>) sent by the instance
  // in |CreateTimers| to timer ids returned in |OnCreateArcTimersDBusMethod|.
  std::map<clockid_t, TimerId> timer_ids_;

  mojo::Binding<mojom::TimerHost> binding_;

  base::WeakPtrFactory<ArcTimerBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcTimerBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TIMER_ARC_TIMER_BRIDGE_H_
