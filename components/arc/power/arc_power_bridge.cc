// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/power/arc_power_bridge.h"

#include <algorithm>
#include <utility>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/system_connector.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/display/manager/display_configurator.h"

namespace arc {
namespace {

// Delay for notifying Android about screen brightness changes, added in
// order to prevent spammy brightness updates.
constexpr base::TimeDelta kNotifyBrightnessDelay =
    base::TimeDelta::FromMilliseconds(200);

// Singleton factory for ArcPowerBridge.
class ArcPowerBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPowerBridge,
          ArcPowerBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPowerBridgeFactory";

  static ArcPowerBridgeFactory* GetInstance() {
    return base::Singleton<ArcPowerBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPowerBridgeFactory>;
  ArcPowerBridgeFactory() = default;
  ~ArcPowerBridgeFactory() override = default;
};

}  // namespace

// WakeLockRequestor requests a wake lock from the device service in response
// to wake lock requests of a given type from Android. A count is kept of
// outstanding Android requests so that only a single actual wake lock is used.
class ArcPowerBridge::WakeLockRequestor {
 public:
  WakeLockRequestor(device::mojom::WakeLockType type,
                    service_manager::Connector* connector)
      : type_(type), connector_(connector) {}
  ~WakeLockRequestor() = default;

  // Increments the number of outstanding requests from Android and requests a
  // wake lock from the device service if this is the only request.
  void AddRequest() {
    num_android_requests_++;
    if (num_android_requests_ > 1)
      return;

    // Initialize |wake_lock_| if this is the first time we're using it.
    if (!wake_lock_) {
      mojo::Remote<device::mojom::WakeLockProvider> provider;
      connector_->Connect(device::mojom::kServiceName,
                          provider.BindNewPipeAndPassReceiver());
      provider->GetWakeLockWithoutContext(
          type_, device::mojom::WakeLockReason::kOther, "ARC",
          wake_lock_.BindNewPipeAndPassReceiver());
    }

    wake_lock_->RequestWakeLock();
  }

  // Decrements the number of outstanding Android requests. Cancels the device
  // service wake lock when the request count hits zero.
  void RemoveRequest() {
    DCHECK_GT(num_android_requests_, 0);
    num_android_requests_--;
    if (num_android_requests_ >= 1)
      return;

    DCHECK(wake_lock_);
    wake_lock_->CancelWakeLock();
  }

  // Runs the message loop until replies have been received for all pending
  // requests on |wake_lock_|.
  void FlushForTesting() {
    if (wake_lock_)
      wake_lock_.FlushForTesting();
  }

 private:
  // Type of wake lock to request.
  device::mojom::WakeLockType type_;

  // Used to get services. Not owned.
  service_manager::Connector* const connector_ = nullptr;

  // Number of outstanding Android requests.
  int num_android_requests_ = 0;

  // Lazily initialized in response to first request.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockRequestor);
};

// static
ArcPowerBridge* ArcPowerBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcPowerBridgeFactory::GetForBrowserContext(context);
}

ArcPowerBridge::ArcPowerBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->power()->SetHost(this);
  arc_bridge_service_->power()->AddObserver(this);
}

ArcPowerBridge::~ArcPowerBridge() {
  arc_bridge_service_->power()->RemoveObserver(this);
  arc_bridge_service_->power()->SetHost(nullptr);
}

bool ArcPowerBridge::TriggerNotifyBrightnessTimerForTesting() {
  if (!notify_brightness_timer_.IsRunning())
    return false;
  notify_brightness_timer_.FireNow();
  return true;
}

void ArcPowerBridge::FlushWakeLocksForTesting() {
  for (const auto& it : wake_lock_requestors_)
    it.second->FlushForTesting();
}

void ArcPowerBridge::OnConnectionReady() {
  // TODO(mash): Support this functionality without ash::Shell access in Chrome.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->display_configurator()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->GetScreenBrightnessPercent(
      base::BindOnce(&ArcPowerBridge::OnGetScreenBrightnessPercent,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcPowerBridge::OnConnectionClosed() {
  // TODO(mash): Support this functionality without ash::Shell access in Chrome.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->display_configurator()->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  wake_lock_requestors_.clear();
}

void ArcPowerBridge::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  mojom::PowerInstance* power_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->power(), Suspend);
  if (!power_instance)
    return;

  auto token = base::UnguessableToken::Create();
  chromeos::PowerManagerClient::Get()->BlockSuspend(token, "ArcPowerBridge");
  power_instance->Suspend(base::BindOnce(
      [](base::UnguessableToken token) {
        chromeos::PowerManagerClient::Get()->UnblockSuspend(token);
      },
      token));
}

void ArcPowerBridge::SuspendDone(const base::TimeDelta& sleep_duration) {
  mojom::PowerInstance* power_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->power(), Resume);
  if (!power_instance)
    return;

  power_instance->Resume();
}

void ArcPowerBridge::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  const base::TimeTicks now = base::TimeTicks::Now();
  if (last_brightness_changed_time_.is_null() ||
      (now - last_brightness_changed_time_) >= kNotifyBrightnessDelay) {
    UpdateAndroidScreenBrightness(change.percent());
    notify_brightness_timer_.Stop();
  } else {
    notify_brightness_timer_.Start(
        FROM_HERE, kNotifyBrightnessDelay,
        base::BindOnce(&ArcPowerBridge::UpdateAndroidScreenBrightness,
                       weak_ptr_factory_.GetWeakPtr(), change.percent()));
  }
  last_brightness_changed_time_ = now;
}

void ArcPowerBridge::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  mojom::PowerInstance* power_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->power(), PowerSupplyInfoChanged);
  if (!power_instance)
    return;

  power_instance->PowerSupplyInfoChanged();
}

void ArcPowerBridge::OnPowerStateChanged(
    chromeos::DisplayPowerState power_state) {
  mojom::PowerInstance* power_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->power(), SetInteractive);
  if (!power_instance)
    return;

  bool enabled = (power_state != chromeos::DISPLAY_POWER_ALL_OFF);
  power_instance->SetInteractive(enabled);
}

void ArcPowerBridge::OnAcquireDisplayWakeLock(mojom::DisplayWakeLockType type) {
  switch (type) {
    case mojom::DisplayWakeLockType::BRIGHT:
      GetWakeLockRequestor(device::mojom::WakeLockType::kPreventDisplaySleep)
          ->AddRequest();
      break;
    case mojom::DisplayWakeLockType::DIM:
      GetWakeLockRequestor(
          device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming)
          ->AddRequest();
      break;
    default:
      LOG(WARNING) << "Tried to take invalid wake lock type "
                   << static_cast<int>(type);
      return;
  }
}

void ArcPowerBridge::OnReleaseDisplayWakeLock(mojom::DisplayWakeLockType type) {
  switch (type) {
    case mojom::DisplayWakeLockType::BRIGHT:
      GetWakeLockRequestor(device::mojom::WakeLockType::kPreventDisplaySleep)
          ->RemoveRequest();
      break;
    case mojom::DisplayWakeLockType::DIM:
      GetWakeLockRequestor(
          device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming)
          ->RemoveRequest();
      break;
    default:
      LOG(WARNING) << "Tried to take invalid wake lock type "
                   << static_cast<int>(type);
      return;
  }
}

void ArcPowerBridge::IsDisplayOn(IsDisplayOnCallback callback) {
  bool is_display_on = false;
  // TODO(mash): Support this functionality without ash::Shell access in Chrome.
  if (ash::Shell::HasInstance())
    is_display_on = ash::Shell::Get()->display_configurator()->IsDisplayOn();
  std::move(callback).Run(is_display_on);
}

void ArcPowerBridge::OnScreenBrightnessUpdateRequest(double percent) {
  power_manager::SetBacklightBrightnessRequest request;
  request.set_percent(percent);
  request.set_transition(
      power_manager::SetBacklightBrightnessRequest_Transition_FAST);
  request.set_cause(
      power_manager::SetBacklightBrightnessRequest_Cause_USER_REQUEST);
  chromeos::PowerManagerClient::Get()->SetScreenBrightness(request);
}

ArcPowerBridge::WakeLockRequestor* ArcPowerBridge::GetWakeLockRequestor(
    device::mojom::WakeLockType type) {
  auto it = wake_lock_requestors_.find(type);
  if (it != wake_lock_requestors_.end())
    return it->second.get();

  service_manager::Connector* connector =
      connector_for_test_ ? connector_for_test_ : content::GetSystemConnector();
  DCHECK(connector);

  it = wake_lock_requestors_
           .emplace(type, std::make_unique<WakeLockRequestor>(type, connector))
           .first;
  return it->second.get();
}

void ArcPowerBridge::OnGetScreenBrightnessPercent(
    base::Optional<double> percent) {
  if (!percent.has_value()) {
    LOG(ERROR)
        << "PowerManagerClient::GetScreenBrightnessPercent reports an error";
    return;
  }
  UpdateAndroidScreenBrightness(percent.value());
}

void ArcPowerBridge::UpdateAndroidScreenBrightness(double percent) {
  mojom::PowerInstance* power_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->power(), UpdateScreenBrightnessSettings);
  if (!power_instance)
    return;
  power_instance->UpdateScreenBrightnessSettings(percent);
}

}  // namespace arc
