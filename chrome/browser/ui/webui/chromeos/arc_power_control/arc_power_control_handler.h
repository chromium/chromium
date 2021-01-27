// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_POWER_CONTROL_ARC_POWER_CONTROL_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_POWER_CONTROL_ARC_POWER_CONTROL_HANDLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/throttle_service.h"
#include "components/arc/mojom/power.mojom.h"
#include "components/arc/power/arc_power_bridge.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"

namespace arc {
class ArcInstanceThrottle;
class ArcSystemStatCollector;
}  // namespace arc

namespace chromeos {

class ArcPowerControlHandler : public content::WebUIMessageHandler,
                               public arc::ArcPowerBridge::Observer,
                               public ThrottleService::ServiceObserver {
 public:
  using WakefulnessModeEvents =
      std::vector<std::pair<base::TimeTicks, arc::mojom::WakefulnessMode>>;
  using ThrottlingEvents =
      std::vector<std::pair<base::TimeTicks, ThrottleObserver::PriorityLevel>>;

  ArcPowerControlHandler();
  ~ArcPowerControlHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // arc::ArcPowerBridge::Observer:
  void OnWakefulnessChanged(arc::mojom::WakefulnessMode mode) override;

  // ThrottleService::ServiceObserver:
  void OnThrottle(ThrottleObserver::PriorityLevel level) override;

 private:
  // Handlers for calls from JS.
  void HandleReady(const base::ListValue* args);
  void HandleSetWakefulnessMode(const base::ListValue* args);
  void HandleSetThrottling(const base::ListValue* args);
  void HandleStartTracing(const base::ListValue* args);
  void HandleStopTracing(const base::ListValue* args);

  void StartTracing();
  void StopTracing();
  void OnTracingModelReady(base::Value result);

  void UpdatePowerControlStatus();
  void SetTracingStatus(const std::string& status);

  void OnIsDeveloperMode(bool developer_mode);

  // Unowned pointers.
  arc::ArcPowerBridge* const power_bridge_;
  arc::ArcInstanceThrottle* const instance_throttle_;

  // Collects system stats runtime.
  base::Time timestamp_;
  base::TimeTicks tracing_time_min_;
  base::OneShotTimer stop_tracing_timer_;
  std::unique_ptr<arc::ArcSystemStatCollector> system_stat_collector_;

  // It collects power mode and throttling events in case tracing is active.
  WakefulnessModeEvents wakefulness_mode_events_;
  ThrottlingEvents throttling_events_;

  // Keeps current wakefulness mode.
  arc::mojom::WakefulnessMode wakefulness_mode_ =
      arc::mojom::WakefulnessMode::UNKNOWN;

  // Enabled in dev mode only.
  bool power_control_enabled_ = false;

  base::WeakPtrFactory<ArcPowerControlHandler> weak_ptr_factory_{this};

  ArcPowerControlHandler(ArcPowerControlHandler const&) = delete;
  ArcPowerControlHandler& operator=(ArcPowerControlHandler const&) = delete;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_POWER_CONTROL_ARC_POWER_CONTROL_HANDLER_H_
