// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
#define COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/mojom/metrics.mojom.h"
#include "components/arc/mojom/process.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/connection_observer.h"
#include "components/guest_os/guest_os_engagement_metrics.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/ozone/gamepad/gamepad_observer.h"
#include "ui/wm/public/activation_change_observer.h"

class BrowserContextKeyedServiceFactory;

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

namespace mojom {
class AppInstance;
class IntentHelperInstance;
}  // namespace mojom

// Collects information from other ArcServices and send UMA metrics.
class ArcMetricsService : public KeyedService,
                          public wm::ActivationChangeObserver,
                          public mojom::MetricsHost,
                          public ui::GamepadObserver {
 public:
  using HistogramNamer =
      base::RepeatingCallback<std::string(const std::string& base_name)>;

  class AppKillObserver : public base::CheckedObserver {
   public:
    virtual void OnArcLowMemoryKill() = 0;
    virtual void OnArcOOMKillCount(unsigned long count) = 0;
    virtual void OnArcMemoryPressureKill(int count, int estimated_freed_kb) = 0;
    virtual void OnArcMetricsServiceDestroyed() {}
  };

  class UserInteractionObserver : public base::CheckedObserver {
   public:
    virtual void OnUserInteraction(UserInteractionType type) = 0;
  };

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcMetricsService* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcMetricsService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // Returns factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  ArcMetricsService(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcMetricsService() override;

  // KeyedService overrides.
  void Shutdown() override;

  // Records one of Arc.UserInteraction UMA stats. |context| cannot be null.
  static void RecordArcUserInteraction(content::BrowserContext* context,
                                       UserInteractionType type);

  // Sets the histogram namer. Required to not have a dependency on browser
  // codebase.
  void SetHistogramNamer(HistogramNamer histogram_namer);

  // Implementations for ConnectionObserver<mojom::ProcessInstance>.
  void OnProcessConnectionReady();
  void OnProcessConnectionClosed();

  // MetricsHost overrides.
  void ReportBootProgress(std::vector<mojom::BootProgressEventPtr> events,
                          mojom::BootType boot_type) override;
  void ReportNativeBridge(mojom::NativeBridgeType native_bridge_type) override;
  void ReportCompanionLibApiUsage(mojom::CompanionLibApiId api_id) override;
  void ReportDnsQueryResult(mojom::ArcDnsQuery query, bool success) override;
  void ReportAppKill(mojom::AppKillPtr app_kill) override;
  void ReportArcCorePriAbiMigEvent(
      mojom::ArcCorePriAbiMigEvent event_type) override;
  void ReportArcCorePriAbiMigFailedTries(uint32_t failed_attempts) override;
  void ReportArcCorePriAbiMigDowngradeDelay(base::TimeDelta delay) override;
  void ReportArcCorePriAbiMigBootTime(base::TimeDelta duration) override;
  void ReportArcSystemHealthUpgrade(base::TimeDelta duration,
                                    bool packages_deleted) override;
  void ReportClipboardDragDropEvent(
      mojom::ArcClipboardDragDropEvent event_type) override;
  void ReportAnr(mojom::AnrPtr anr) override;
  void ReportLowLatencyStylusLibApiUsage(
      mojom::LowLatencyStylusLibApiId api_id) override;
  void ReportLowLatencyStylusLibPredictionTarget(
      mojom::LowLatencyStylusLibPredictionTargetPtr prediction_target) override;
  void ReportEntireFixupMetrics(base::TimeDelta duration,
                                uint32_t number_of_directories,
                                uint32_t number_of_failures) override;
  void ReportPerAppFixupMetrics(base::TimeDelta duration,
                                uint32_t number_of_directories) override;

  // wm::ActivationChangeObserver overrides.
  // Records to UMA when a user has interacted with an ARC app window.
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // ui::GamepadObserver overrides.
  void OnGamepadEvent(const ui::GamepadEvent& event) override;

  // ArcAppListPrefs::Observer callbacks which are called through
  // ArcMetricsServiceProxy.
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent);
  void OnTaskDestroyed(int32_t task_id);

  void AddAppKillObserver(AppKillObserver* obs);
  void RemoveAppKillObserver(AppKillObserver* obs);

  void AddUserInteractionObserver(UserInteractionObserver* obs);
  void RemoveUserInteractionObserver(UserInteractionObserver* obs);

  // Finds the boot_progress_arc_upgraded event, removes it from |events|, and
  // returns the event time. If the boot_progress_arc_upgraded event is not
  // found, absl::nullopt is returned. This function is public for testing
  // purposes.
  absl::optional<base::TimeTicks> GetArcStartTimeFromEvents(
      std::vector<mojom::BootProgressEventPtr>& events);

  // Forwards reports of app kills resulting from a MemoryPressureArcvm signal
  // to MemoryKillsMonitor via ArcMetricsServiceProxy.
  void ReportMemoryPressureArcVmKills(int count, int estimated_freed_kb);

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

  class ArcBridgeServiceObserver : public arc::ArcBridgeService::Observer {
   public:
    ArcBridgeServiceObserver();
    ~ArcBridgeServiceObserver() override;

    // Whether the arc bridge is in the process of closing.
    bool arc_bridge_closing_ = false;

   private:
    // arc::ArcBridgeService::Observer overrides.
    void BeforeArcBridgeClosed() override;
    void AfterArcBridgeClosed() override;
    DISALLOW_COPY_AND_ASSIGN(ArcBridgeServiceObserver);
  };

  class IntentHelperObserver
      : public ConnectionObserver<mojom::IntentHelperInstance> {
   public:
    IntentHelperObserver(ArcMetricsService* arc_metrics_service,
                         ArcBridgeServiceObserver* arc_bridge_service_observer);
    ~IntentHelperObserver() override;

   private:
    // arc::internal::ConnectionObserver<mojom::IntentHelperInstance>
    // overrides.
    void OnConnectionClosed() override;

    ArcMetricsService* arc_metrics_service_;
    ArcBridgeServiceObserver* arc_bridge_service_observer_;

    DISALLOW_COPY_AND_ASSIGN(IntentHelperObserver);
  };

  class AppLauncherObserver : public ConnectionObserver<mojom::AppInstance> {
   public:
    AppLauncherObserver(ArcMetricsService* arc_metrics_service,
                        ArcBridgeServiceObserver* arc_bridge_service_observer);
    ~AppLauncherObserver() override;

   private:
    // arc::internal::ConnectionObserver<mojom::IntentHelperInstance>
    // overrides.
    void OnConnectionClosed() override;

    ArcMetricsService* arc_metrics_service_;
    ArcBridgeServiceObserver* arc_bridge_service_observer_;

    DISALLOW_COPY_AND_ASSIGN(AppLauncherObserver);
  };

  void RecordArcUserInteraction(UserInteractionType type);
  void RequestProcessList();
  void ParseProcessList(std::vector<mojom::RunningAppProcessInfoPtr> processes);

  // DBus callbacks.
  void OnArcStartTimeRetrieved(std::vector<mojom::BootProgressEventPtr> events,
                               mojom::BootType boot_type,
                               absl::optional<base::TimeTicks> arc_start_time);
  void OnArcStartTimeForPriAbiMigration(
      base::TimeTicks durationTicks,
      absl::optional<base::TimeTicks> arc_start_time);

  // Notify AppKillObservers.
  void NotifyLowMemoryKill();
  void NotifyOOMKillCount(unsigned long count);

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // Helper class for tracking engagement metrics.
  guest_os::GuestOsEngagementMetrics guest_os_engagement_metrics_;

  // A function that appends a suffix to the base of a histogram name based on
  // the current user profile.
  HistogramNamer histogram_namer_;

  ProcessObserver process_observer_;
  base::RepeatingTimer request_process_list_timer_;

  ArcBridgeServiceObserver arc_bridge_service_observer_;
  IntentHelperObserver intent_helper_observer_;
  AppLauncherObserver app_launcher_observer_;

  bool was_arc_window_active_ = false;
  std::vector<int32_t> task_ids_;

  bool gamepad_interaction_recorded_ = false;

  base::ObserverList<AppKillObserver> app_kill_observers_;
  base::ObserverList<UserInteractionObserver> user_interaction_observers_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcMetricsService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcMetricsService);
};

// Singleton factory for ArcMetricsService.
class ArcMetricsServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMetricsService,
          ArcMetricsServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMetricsServiceFactory";

  static ArcMetricsServiceFactory* GetInstance();

 private:
  friend base::DefaultSingletonTraits<ArcMetricsServiceFactory>;
  ArcMetricsServiceFactory() = default;
  ~ArcMetricsServiceFactory() override = default;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
