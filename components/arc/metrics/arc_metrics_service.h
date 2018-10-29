// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
#define COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "components/arc/common/metrics.mojom.h"
#include "components/arc/common/process.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Collects information from other ArcServices and send UMA metrics.
class ArcMetricsService : public KeyedService,
                          public wm::ActivationChangeObserver,
                          public mojom::MetricsHost {
 public:
  // These values are persisted to logs, and should therefore never be
  // renumbered nor reused. They are public for testing only.
  enum class NativeBridgeType {
    // Native bridge value has not been received from the container yet.
    UNKNOWN = 0,
    // Native bridge is not used.
    NONE = 1,
    // Using houdini translator.
    HOUDINI = 2,
    // Using ndk-translation translator.
    NDK_TRANSLATION = 3,
    kMaxValue = NDK_TRANSLATION,
  };

  // Delegate for handling window focus observation that is used to track ARC
  // app usage metrics.
  class ArcWindowDelegate {
   public:
    virtual ~ArcWindowDelegate() = default;
    // Returns whether |window| is an ARC window.
    virtual bool IsArcAppWindow(const aura::Window* window) const = 0;
    virtual void RegisterActivationChangeObserver() = 0;
    virtual void UnregisterActivationChangeObserver() = 0;
  };

  // Sets the fake ArcWindowDelegate for testing.
  void SetArcWindowDelegateForTesting(
      std::unique_ptr<ArcWindowDelegate> delegate);

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcMetricsService* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcMetricsService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcMetricsService(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcMetricsService() override;

  // Implementations for ConnectionObserver<mojom::ProcessInstance>.
  void OnProcessConnectionReady();
  void OnProcessConnectionClosed();

  // MetricsHost overrides.
  void ReportBootProgress(std::vector<mojom::BootProgressEventPtr> events,
                          mojom::BootType boot_type) override;
  void ReportNativeBridge(mojom::NativeBridgeType native_bridge_type) override;

  // Records native bridge UMA according to value received from the
  // container or as UNKNOWN if the value has not been recieved yet.
  void RecordNativeBridgeUMA();

  // wm::ActivationChangeObserver overrides.
  // Records to UMA when a user has interacted with an ARC app window.
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  NativeBridgeType native_bridge_type_for_testing() const {
    return native_bridge_type_;
  }

 private:
  // Adapter to be able to also observe ProcessInstance events.
  class ProcessObserver : public ConnectionObserver<mojom::ProcessInstance> {
   public:
    explicit ProcessObserver(ArcMetricsService* arc_metrics_service);
    ~ProcessObserver() override;

   private:
    // ConnectionObserver<mojom::ProcessInstance> overrides.
    void OnConnectionReady() override;
    void OnConnectionClosed() override;

    ArcMetricsService* arc_metrics_service_;

    DISALLOW_COPY_AND_ASSIGN(ProcessObserver);
  };

  void RequestProcessList();
  void ParseProcessList(std::vector<mojom::RunningAppProcessInfoPtr> processes);

  // DBus callbacks.
  void OnArcStartTimeRetrieved(std::vector<mojom::BootProgressEventPtr> events,
                               mojom::BootType boot_type,
                               base::Optional<base::TimeTicks> arc_start_time);

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  std::unique_ptr<ArcWindowDelegate> arc_window_delegate_;

  ProcessObserver process_observer_;
  base::RepeatingTimer timer_;

  NativeBridgeType native_bridge_type_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcMetricsService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcMetricsService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
