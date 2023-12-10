// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/arc_power_control/arc_power_control_handler.h"

#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "base/linux_util.h"
#include "base/process/launch.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_instance_throttle.h"
#include "chrome/browser/ash/arc/tracing/arc_system_stat_collector.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_model.h"
#include "chrome/browser/ash/arc/tracing/arc_value_event_trimmer.h"
#include "chrome/browser/ash/arc/tracing/present_frames_tracer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace ash {

namespace {

// Maximum interval to display in tracing.
constexpr base::TimeDelta kMaxIntervalToDisplay = base::Minutes(5);

// Names of throttling mode.
constexpr char kThrottlingDisable[] = "disable";
constexpr char kThrottlingAuto[] = "auto";
constexpr char kThrottlingForce[] = "force";

// Names of wakenessfull mode.
constexpr char kWakenessfullWakeUp[] = "wakeup";
constexpr char kWakenessfullDoze[] = "doze";
constexpr char kWakenessfullForceDoze[] = "force-doze";
constexpr char kWakenessfullSleep[] = "sleep";

// To read developer mode.
constexpr const char kCrosSystemPath[] = "/usr/bin/crossystem";
constexpr const char kCrosDebug[] = "cros_debug";

void OnAndroidSuspendReady() {}

std::string GetJavascriptDomain() {
  return "cr.ArcPowerControl.";
}

bool IsDeveloperMode() {
  std::string output;
  if (!base::GetAppOutput({kCrosSystemPath, kCrosDebug}, &output)) {
    LOG(ERROR) << "Failed to read property " << kCrosDebug;
    return false;
  }

  return output == "1";
}

base::Value BuildTracingModel(
    base::Time timestamp,
    base::TimeTicks time_min,
    base::TimeTicks time_max,
    std::unique_ptr<arc::ArcSystemStatCollector> system_stat_collector,
    ArcPowerControlHandler::WakefulnessModeEvents wakefulness_mode_events,
    ArcPowerControlHandler::ThrottlingEvents throttling_events) {
  DCHECK(system_stat_collector);

  arc::ArcTracingModel common_model;
  const base::TimeTicks time_min_clamped =
      std::max(time_min, time_max - system_stat_collector->max_interval());
  common_model.SetMinMaxTime(
      (time_min_clamped - base::TimeTicks()).InMicroseconds(),
      (time_max - base::TimeTicks()).InMicroseconds());

  arc::ArcValueEventTrimmer wakenessfull_mode(
      &common_model.system_model().memory_events(),
      arc::ArcValueEvent::Type::kWakenessfullMode);
  for (const auto& wakefulness_mode_event : wakefulness_mode_events) {
    const int64_t wakefulness_mode_timestamp =
        (wakefulness_mode_event.first - base::TimeTicks()).InMicroseconds();
    wakenessfull_mode.MaybeAdd(wakefulness_mode_timestamp,
                               static_cast<int>(wakefulness_mode_event.second));
  }
  arc::ArcValueEventTrimmer throttling(
      &common_model.system_model().memory_events(),
      arc::ArcValueEvent::Type::kThrottlingMode);
  for (const auto& throttling_event : throttling_events) {
    const int64_t throttling_timestamp =
        (throttling_event.first - base::TimeTicks()).InMicroseconds();
    throttling.MaybeAdd(throttling_timestamp,
                        static_cast<int>(throttling_event.second));
  }

  // Flush automatically normalizes the model.
  system_stat_collector->Flush(time_min, time_max,
                               &common_model.system_model());

  arc::ArcTracingGraphicsModel graphics_model;
  graphics_model.set_skip_structure_validation();
  graphics_model.set_platform(base::GetLinuxDistro());
  graphics_model.set_timestamp(timestamp);
  graphics_model.Build(common_model, arc::PresentFramesTracer() /* commits */);

  return base::Value(graphics_model.Serialize());
}

}  // namespace

ArcPowerControlHandler::ArcPowerControlHandler()
    : power_bridge_(arc::ArcPowerBridge::GetForBrowserContext(
          ProfileManager::GetActiveUserProfile())),
      instance_throttle_(arc::ArcInstanceThrottle::GetForBrowserContext(
          ProfileManager::GetActiveUserProfile())) {
  DCHECK(power_bridge_);
  DCHECK(instance_throttle_);
  power_bridge_->AddObserver(this);
  instance_throttle_->AddServiceObserver(this);
}

ArcPowerControlHandler::~ArcPowerControlHandler() {
  instance_throttle_->RemoveServiceObserver(this);
  power_bridge_->RemoveObserver(this);
}

void ArcPowerControlHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "ready", base::BindRepeating(&ArcPowerControlHandler::HandleReady,
                                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setWakefulnessMode",
      base::BindRepeating(&ArcPowerControlHandler::HandleSetWakefulnessMode,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setThrottling",
      base::BindRepeating(&ArcPowerControlHandler::HandleSetThrottling,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startTracing",
      base::BindRepeating(&ArcPowerControlHandler::HandleStartTracing,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stopTracing",
      base::BindRepeating(&ArcPowerControlHandler::HandleStopTracing,
                          base::Unretained(this)));
}

void ArcPowerControlHandler::OnWakefulnessChanged(
    arc::mojom::WakefulnessMode mode) {
  wakefulness_mode_ = mode;
  UpdatePowerControlStatus();

  if (stop_tracing_timer_.IsRunning()) {
    wakefulness_mode_events_.emplace_back(
        std::make_pair(TRACE_TIME_TICKS_NOW(), wakefulness_mode_));
  }
}

void ArcPowerControlHandler::OnThrottle(bool throttled) {
  UpdatePowerControlStatus();

  std::string mode = kThrottlingAuto;
  auto* observer = instance_throttle_->GetObserverByName(
      arc::ArcInstanceThrottle::kChromeArcPowerControlPageObserver);
  if (observer && observer->enforced())
    mode = observer->active() ? kThrottlingDisable : kThrottlingForce;

  CallJavascriptFunction(GetJavascriptDomain() + "setThrottlingMode",
                         base::Value(mode));

  if (stop_tracing_timer_.IsRunning()) {
    throttling_events_.emplace_back(
        std::make_pair(TRACE_TIME_TICKS_NOW(), throttled));
  }
}

void ArcPowerControlHandler::HandleReady(const base::Value::List& args) {
  arc::mojom::PowerInstance* power_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc::ArcServiceManager::Get()->arc_bridge_service()->power(),
      GetWakefulnessMode);
  if (!power_instance)
    return;

  power_instance->GetWakefulnessMode(
      base::BindOnce(&ArcPowerControlHandler::OnWakefulnessChanged,
                     weak_ptr_factory_.GetWeakPtr()));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&IsDeveloperMode),
      base::BindOnce(&ArcPowerControlHandler::OnIsDeveloperMode,
                     weak_ptr_factory_.GetWeakPtr()));

  OnThrottle(instance_throttle_->should_throttle());
}

void ArcPowerControlHandler::HandleSetWakefulnessMode(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());

  if (!power_control_enabled_) {
    LOG(ERROR) << "Power control is not enabled";
    return;
  }

  if (!args[0].is_string()) {
    LOG(ERROR) << "Invalid input";
    return;
  }

  auto* const power =
      arc::ArcServiceManager::Get()->arc_bridge_service()->power();
  DCHECK(power);

  const std::string mode = args[0].GetString();
  if (mode == kWakenessfullWakeUp) {
    if (wakefulness_mode_ == arc::mojom::WakefulnessMode::ASLEEP) {
      arc::mojom::PowerInstance* const power_instance =
          ARC_GET_INSTANCE_FOR_METHOD(power, Resume);
      if (power_instance)
        power_instance->Resume();
    } else {
      arc::mojom::PowerInstance* const power_instance =
          ARC_GET_INSTANCE_FOR_METHOD(power, SetIdleState);
      if (power_instance)
        power_instance->SetIdleState(arc::mojom::IdleState::ACTIVE);
    }
  } else if (mode == kWakenessfullDoze) {
    arc::mojom::PowerInstance* const power_instance =
        ARC_GET_INSTANCE_FOR_METHOD(power, SetIdleState);
    if (power_instance)
      power_instance->SetIdleState(arc::mojom::IdleState::INACTIVE);
  } else if (mode == kWakenessfullForceDoze) {
    arc::mojom::PowerInstance* const power_instance =
        ARC_GET_INSTANCE_FOR_METHOD(power, SetIdleState);
    if (power_instance) {
      power_instance->SetIdleState(arc::mojom::IdleState::FORCE_INACTIVE);
    }
  } else if (mode == kWakenessfullSleep) {
    arc::mojom::PowerInstance* const power_instance =
        ARC_GET_INSTANCE_FOR_METHOD(power, Suspend);
    if (power_instance)
      power_instance->Suspend(base::BindOnce(&OnAndroidSuspendReady));
  } else {
    LOG(ERROR) << "Invalid mode: " << mode;
  }
}

void ArcPowerControlHandler::HandleSetThrottling(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());

  if (!power_control_enabled_) {
    LOG(ERROR) << "Power control is not enabled";
    return;
  }

  if (!args[0].is_string()) {
    LOG(ERROR) << "Invalid input";
    return;
  }

  auto* observer = instance_throttle_->GetObserverByName(
      arc::ArcInstanceThrottle::kChromeArcPowerControlPageObserver);
  if (!observer) {
    LOG(ERROR)
        << "An throttle observer for chrome://arc-power-control not found";
    return;
  }

  const std::string mode = args[0].GetString();
  if (mode == kThrottlingDisable) {
    observer->SetActive(true);
    observer->SetEnforced(true);
  } else if (mode == kThrottlingAuto) {
    observer->SetActive(false);
    observer->SetEnforced(false);
  } else if (mode == kThrottlingForce) {
    observer->SetActive(false);
    observer->SetEnforced(true);
  } else {
    LOG(ERROR) << "Invalid mode: " << mode;
    return;
  }
}

void ArcPowerControlHandler::HandleStartTracing(const base::Value::List& args) {
  DCHECK(!args.size());
  StartTracing();
}

void ArcPowerControlHandler::HandleStopTracing(const base::Value::List& args) {
  DCHECK(!args.size());
  StopTracing();
}

void ArcPowerControlHandler::StartTracing() {
  SetTracingStatus("Collecting samples...");

  timestamp_ = base::Time::Now();
  tracing_time_min_ = TRACE_TIME_TICKS_NOW();

  wakefulness_mode_events_.clear();
  wakefulness_mode_events_.emplace_back(
      std::make_pair(tracing_time_min_, wakefulness_mode_));
  throttling_events_.clear();
  throttling_events_.emplace_back(
      std::make_pair(tracing_time_min_, instance_throttle_->should_throttle()));
  system_stat_collector_ = std::make_unique<arc::ArcSystemStatCollector>();
  system_stat_collector_->Start(kMaxIntervalToDisplay);
  stop_tracing_timer_.Start(FROM_HERE, system_stat_collector_->max_interval(),
                            base::BindOnce(&ArcPowerControlHandler::StopTracing,
                                           base::Unretained(this)));
}

void ArcPowerControlHandler::StopTracing() {
  if (!system_stat_collector_)
    return;

  const base::TimeTicks tracing_time_max = TRACE_TIME_TICKS_NOW();
  stop_tracing_timer_.Stop();
  system_stat_collector_->Stop();

  SetTracingStatus("Building model...");

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BuildTracingModel, timestamp_, tracing_time_min_,
                     tracing_time_max, std::move(system_stat_collector_),
                     std::move(wakefulness_mode_events_),
                     std::move(throttling_events_)),
      base::BindOnce(&ArcPowerControlHandler::OnTracingModelReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcPowerControlHandler::OnTracingModelReady(base::Value result) {
  SetTracingStatus("Tracing ready");

  CallJavascriptFunction(GetJavascriptDomain() + "setModel", std::move(result));
}

void ArcPowerControlHandler::OnIsDeveloperMode(bool developer_mode) {
  power_control_enabled_ = developer_mode;
  CallJavascriptFunction(GetJavascriptDomain() + "setPowerControlEnabled",
                         base::Value(power_control_enabled_));
}

void ArcPowerControlHandler::UpdatePowerControlStatus() {
  AllowJavascript();

  std::string status;
  switch (wakefulness_mode_) {
    case arc::mojom::WakefulnessMode::UNKNOWN:
      status = "Unknown";
      break;
    case arc::mojom::WakefulnessMode::ASLEEP:
      status = "Asleep";
      break;
    case arc::mojom::WakefulnessMode::AWAKE:
      status = "Awake";
      break;
    case arc::mojom::WakefulnessMode::DREAMING:
      status = "Dreaming";
      break;
    case arc::mojom::WakefulnessMode::DOZING:
      status = "Dozing";
      break;
  }

  status += " (";
  if (instance_throttle_->should_throttle())
    status += "throttling";
  else
    status += "critical foreground";
  status += ")";

  CallJavascriptFunction(GetJavascriptDomain() + "setPowerControlStatus",
                         base::Value(status));
}

void ArcPowerControlHandler::SetTracingStatus(const std::string& status) {
  CallJavascriptFunction(GetJavascriptDomain() + "setTracingStatus",
                         base::Value(status));
}

}  // namespace ash
