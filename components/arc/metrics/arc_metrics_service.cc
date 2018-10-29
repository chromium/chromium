// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/metrics/arc_metrics_service.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/exo/wm_helper.h"

namespace arc {

namespace {

constexpr base::TimeDelta kUmaMinTime = base::TimeDelta::FromMilliseconds(1);
constexpr base::TimeDelta kUmaMaxTime = base::TimeDelta::FromSeconds(60);
constexpr int kUmaNumBuckets = 50;

constexpr base::TimeDelta kRequestProcessListPeriod =
    base::TimeDelta::FromMinutes(5);
constexpr char kArcProcessNamePrefix[] = "org.chromium.arc.";
constexpr char kGmsProcessNamePrefix[] = "com.google.android.gms";
constexpr char kBootProgressEnableScreen[] = "boot_progress_enable_screen";

std::string BootTypeToString(mojom::BootType boot_type) {
  switch (boot_type) {
    case mojom::BootType::UNKNOWN:
      break;
    case mojom::BootType::FIRST_BOOT:
      return ".FirstBoot";
    case mojom::BootType::FIRST_BOOT_AFTER_UPDATE:
      return ".FirstBootAfterUpdate";
    case mojom::BootType::REGULAR_BOOT:
      return ".RegularBoot";
  }
  NOTREACHED();
  return "";
}

class ArcWindowDelegateImpl : public ArcMetricsService::ArcWindowDelegate {
 public:
  explicit ArcWindowDelegateImpl(ArcMetricsService* service)
      : service_(service) {}

  ~ArcWindowDelegateImpl() override = default;

  bool IsArcAppWindow(const aura::Window* window) const override {
    return arc::IsArcAppWindow(window);
  }

  void RegisterActivationChangeObserver() override {
    // If WMHelper doesn't exist, do nothing. This occurs in tests.
    if (exo::WMHelper::HasInstance())
      exo::WMHelper::GetInstance()->AddActivationObserver(service_);
  }

  void UnregisterActivationChangeObserver() override {
    // If WMHelper is already destroyed, do nothing.
    // TODO(crbug.com/748380): Fix shutdown order.
    if (exo::WMHelper::HasInstance())
      exo::WMHelper::GetInstance()->RemoveActivationObserver(service_);
  }

 private:
  ArcMetricsService* const service_;  // Owned by ArcMetricsService

  DISALLOW_COPY_AND_ASSIGN(ArcWindowDelegateImpl);
};

// Singleton factory for ArcMetricsService.
class ArcMetricsServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMetricsService,
          ArcMetricsServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMetricsServiceFactory";

  static ArcMetricsServiceFactory* GetInstance() {
    return base::Singleton<ArcMetricsServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcMetricsServiceFactory>;
  ArcMetricsServiceFactory() = default;
  ~ArcMetricsServiceFactory() override = default;
};

}  // namespace

// static
ArcMetricsService* ArcMetricsService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMetricsServiceFactory::GetForBrowserContext(context);
}

// static
ArcMetricsService* ArcMetricsService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcMetricsServiceFactory::GetForBrowserContextForTesting(context);
}

ArcMetricsService::ArcMetricsService(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      arc_window_delegate_(std::make_unique<ArcWindowDelegateImpl>(this)),
      process_observer_(this),
      native_bridge_type_(NativeBridgeType::UNKNOWN),
      weak_ptr_factory_(this) {
  arc_bridge_service_->metrics()->SetHost(this);
  arc_bridge_service_->process()->AddObserver(&process_observer_);
  arc_window_delegate_->RegisterActivationChangeObserver();
}

ArcMetricsService::~ArcMetricsService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service_->process()->RemoveObserver(&process_observer_);
  arc_bridge_service_->metrics()->SetHost(nullptr);
  arc_window_delegate_->UnregisterActivationChangeObserver();
}

void ArcMetricsService::SetArcWindowDelegateForTesting(
    std::unique_ptr<ArcWindowDelegate> delegate) {
  arc_window_delegate_ = std::move(delegate);
}

void ArcMetricsService::OnProcessConnectionReady() {
  VLOG(2) << "Start updating process list.";
  timer_.Start(FROM_HERE, kRequestProcessListPeriod, this,
               &ArcMetricsService::RequestProcessList);
}

void ArcMetricsService::OnProcessConnectionClosed() {
  VLOG(2) << "Stop updating process list.";
  timer_.Stop();
}

void ArcMetricsService::RequestProcessList() {
  mojom::ProcessInstance* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->process(), RequestProcessList);
  if (!process_instance)
    return;
  VLOG(2) << "RequestProcessList";
  process_instance->RequestProcessList(base::BindOnce(
      &ArcMetricsService::ParseProcessList, weak_ptr_factory_.GetWeakPtr()));
}

void ArcMetricsService::ParseProcessList(
    std::vector<mojom::RunningAppProcessInfoPtr> processes) {
  int running_app_count = 0;
  for (const auto& process : processes) {
    const std::string& process_name = process->process_name;
    const mojom::ProcessState& process_state = process->process_state;

    // Processes like the ARC launcher and intent helper are always running
    // and not counted as apps running by users. With the same reasoning,
    // GMS (Google Play Services) and its related processes are skipped as
    // well. The process_state check below filters out system processes,
    // services, apps that are cached because they've run before.
    if (base::StartsWith(process_name, kArcProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(process_name, kGmsProcessNamePrefix,
                         base::CompareCase::SENSITIVE) ||
        process_state != mojom::ProcessState::TOP) {
      VLOG(2) << "Skipped " << process_name << " " << process_state;
    } else {
      ++running_app_count;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Arc.AppCount", running_app_count);
}

void ArcMetricsService::OnArcStartTimeRetrieved(
    std::vector<mojom::BootProgressEventPtr> events,
    mojom::BootType boot_type,
    base::Optional<base::TimeTicks> arc_start_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!arc_start_time.has_value()) {
    LOG(ERROR) << "Failed to retrieve ARC start timeticks.";
    return;
  }
  VLOG(2) << "ARC start @" << arc_start_time.value();

  DCHECK_NE(mojom::BootType::UNKNOWN, boot_type);
  const std::string suffix = BootTypeToString(boot_type);
  for (const auto& event : events) {
    VLOG(2) << "Report boot progress event:" << event->event << "@"
            << event->uptimeMillis;
    const std::string name = "Arc." + event->event + suffix;
    const base::TimeTicks uptime =
        base::TimeDelta::FromMilliseconds(event->uptimeMillis) +
        base::TimeTicks();
    const base::TimeDelta elapsed_time = uptime - arc_start_time.value();
    base::UmaHistogramCustomTimes(name, elapsed_time, kUmaMinTime, kUmaMaxTime,
                                  kUmaNumBuckets);
    if (event->event.compare(kBootProgressEnableScreen) == 0) {
      base::UmaHistogramCustomTimes("Arc.AndroidBootTime" + suffix,
                                    elapsed_time, kUmaMinTime, kUmaMaxTime,
                                    kUmaNumBuckets);
    }
  }
}

void ArcMetricsService::ReportBootProgress(
    std::vector<mojom::BootProgressEventPtr> events,
    mojom::BootType boot_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (boot_type == mojom::BootType::UNKNOWN) {
    LOG(WARNING) << "boot_type is unknown. Skip recording UMA.";
    return;
  }

  // Retrieve ARC start time from session manager.
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->GetArcStartTime(base::BindOnce(
      &ArcMetricsService::OnArcStartTimeRetrieved,
      weak_ptr_factory_.GetWeakPtr(), std::move(events), boot_type));
}

void ArcMetricsService::ReportNativeBridge(
    mojom::NativeBridgeType native_bridge_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "Mojo native bridge type is " << native_bridge_type;

  // Save value for RecordNativeBridgeUMA instead of recording
  // immediately since it must appear in every metrics interval
  // uploaded to UMA.
  switch (native_bridge_type) {
    case mojom::NativeBridgeType::NONE:
      native_bridge_type_ = NativeBridgeType::NONE;
      return;
    case mojom::NativeBridgeType::HOUDINI:
      native_bridge_type_ = NativeBridgeType::HOUDINI;
      return;
    case mojom::NativeBridgeType::NDK_TRANSLATION:
      native_bridge_type_ = NativeBridgeType::NDK_TRANSLATION;
      return;
  }
  NOTREACHED() << native_bridge_type;
}

void ArcMetricsService::RecordNativeBridgeUMA() {
  UMA_HISTOGRAM_ENUMERATION("Arc.NativeBridge", native_bridge_type_);
}

void ArcMetricsService::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!arc_window_delegate_->IsArcAppWindow(gained_active))
    return;
  UMA_HISTOGRAM_ENUMERATION(
      "Arc.UserInteraction",
      UserInteractionType::APP_CONTENT_WINDOW_INTERACTION);
}

ArcMetricsService::ProcessObserver::ProcessObserver(
    ArcMetricsService* arc_metrics_service)
    : arc_metrics_service_(arc_metrics_service) {}

ArcMetricsService::ProcessObserver::~ProcessObserver() = default;

void ArcMetricsService::ProcessObserver::OnConnectionReady() {
  arc_metrics_service_->OnProcessConnectionReady();
}

void ArcMetricsService::ProcessObserver::OnConnectionClosed() {
  arc_metrics_service_->OnProcessConnectionClosed();
}

}  // namespace arc
