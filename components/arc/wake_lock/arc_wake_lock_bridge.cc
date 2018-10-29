// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/task/post_task.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/wake_lock/arc_wake_lock_bridge.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace arc {

namespace {

constexpr char kWakeLockReason[] = "ARC";

// Singleton factory for ArcWakeLockBridge.
class ArcWakeLockBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcWakeLockBridge,
          ArcWakeLockBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcWakeLockBridgeFactory";

  static ArcWakeLockBridgeFactory* GetInstance() {
    return base::Singleton<ArcWakeLockBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcWakeLockBridgeFactory>;
  ArcWakeLockBridgeFactory() = default;
  ~ArcWakeLockBridgeFactory() override = default;
};

}  // namespace

// WakeLockRequester requests a wake lock from the device service in response
// to wake lock requests of a given type from Android. A count is kept of
// outstanding Android requests so that only a single actual wake lock is
// used.
class ArcWakeLockBridge::WakeLockRequester {
 public:
  WakeLockRequester(device::mojom::WakeLockType type,
                    service_manager::Connector* connector)
      : type_(type), connector_(connector) {}
  ~WakeLockRequester() = default;

  // Increments the number of outstanding requests from Android and requests a
  // wake lock from the device service if this is the only request.
  void AddRequest() {
    DCHECK_GE(wake_lock_count_, 0);
    wake_lock_count_++;
    if (wake_lock_count_ > 1) {
      DVLOG(1) << "Partial wake lock acquire. Count: " << wake_lock_count_;
      return;
    }

    // Initialize |wake_lock_| if this is the first time we're using it.
    DVLOG(1) << "Partial wake lock new acquire. Count: " << wake_lock_count_;
    if (!wake_lock_) {
      device::mojom::WakeLockProviderPtr provider;
      connector_->BindInterface(device::mojom::kServiceName,
                                mojo::MakeRequest(&provider));
      provider->GetWakeLockWithoutContext(
          type_, device::mojom::WakeLockReason::kOther, kWakeLockReason,
          mojo::MakeRequest(&wake_lock_));
    }

    wake_lock_->RequestWakeLock();

    for (auto& observer : observers_)
      observer.OnWakeLockAcquire();
  }

  // Decrements the number of outstanding Android requests. Cancels the device
  // service wake lock when the request count hits zero.
  void RemoveRequest() {
    DCHECK_GE(wake_lock_count_, 0);
    if (wake_lock_count_ == 0) {
      LOG(WARNING) << "Release without acquire. Count: " << wake_lock_count_;
      return;
    }

    wake_lock_count_--;
    if (wake_lock_count_ >= 1) {
      DVLOG(1) << "Partial wake release. Count: " << wake_lock_count_;
      return;
    }

    DCHECK(wake_lock_);
    DVLOG(1) << "Partial wake lock force release. Count: " << wake_lock_count_;
    wake_lock_->CancelWakeLock();

    for (auto& observer : observers_)
      observer.OnWakeLockRelease();
  }

  bool IsWakeLockHeld() const { return wake_lock_count_ > 0; }

  void AddObserver(WakeLockObserver* observer) {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(WakeLockObserver* observer) {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  bool HasObservers() const { return observers_.might_have_observers(); }

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
  int64_t wake_lock_count_ = 0;

  // Lazily initialized in response to first request.
  device::mojom::WakeLockPtr wake_lock_;

  base::ObserverList<WakeLockObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockRequester);
};

// static
BrowserContextKeyedServiceFactory* ArcWakeLockBridge::GetFactory() {
  return ArcWakeLockBridgeFactory::GetInstance();
}

// static
ArcWakeLockBridge* ArcWakeLockBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcWakeLockBridgeFactory::GetForBrowserContext(context);
}

// static
ArcWakeLockBridge* ArcWakeLockBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcWakeLockBridgeFactory::GetForBrowserContextForTesting(context);
}

constexpr base::TimeDelta ArcWakeLockBridge::kDarkResumeWakeLockCheckTimeout;
constexpr base::TimeDelta ArcWakeLockBridge::kDarkResumeHardTimeout;

ArcWakeLockBridge::ArcWakeLockBridge(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      binding_(this),
      weak_ptr_factory_(this) {
  arc_bridge_service_->wake_lock()->SetHost(this);
  arc_bridge_service_->wake_lock()->AddObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
}

ArcWakeLockBridge::~ArcWakeLockBridge() {
  arc_bridge_service_->wake_lock()->RemoveObserver(this);
  arc_bridge_service_->wake_lock()->SetHost(nullptr);
  // In case some this wasn't cleared while handling a dark resume.
  GetWakeLockRequester(device::mojom::WakeLockType::kPreventAppSuspension)
      ->RemoveObserver(this);
}

void ArcWakeLockBridge::OnConnectionClosed() {
  DVLOG(1) << "OnConnectionClosed";
  wake_lock_requesters_.clear();
}

void ArcWakeLockBridge::AcquirePartialWakeLock(
    AcquirePartialWakeLockCallback callback) {
  GetWakeLockRequester(device::mojom::WakeLockType::kPreventAppSuspension)
      ->AddRequest();
  std::move(callback).Run(true);
}

void ArcWakeLockBridge::ReleasePartialWakeLock(
    ReleasePartialWakeLockCallback callback) {
  GetWakeLockRequester(device::mojom::WakeLockType::kPreventAppSuspension)
      ->RemoveRequest();
  std::move(callback).Run(true);
}

void ArcWakeLockBridge::DarkSuspendImminent() {
  DVLOG(1) << __func__;
  suspend_readiness_cb_ = chromeos::DBusThreadManager::Get()
                              ->GetPowerManagerClient()
                              ->GetSuspendReadinessCallback(FROM_HERE);
  // Post task that will check for any wake locks acquired in dark resume.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcWakeLockBridge::HandleDarkResumeWakeLockCheckTimeout,
                     dark_resume_weak_ptr_factory_.GetWeakPtr()),
      kDarkResumeWakeLockCheckTimeout);
}

void ArcWakeLockBridge::SuspendDone(const base::TimeDelta& sleep_duration) {
  DVLOG(1) << __func__;
  // Clear any dark resume state when the device resumes.
  ClearDarkResumeState();
}

void ArcWakeLockBridge::OnWakeLockRelease() {
  // This observer is only registered once dark resume starts.
  DCHECK(suspend_readiness_cb_);
  DVLOG(1) << __func__;

  // At this point the instance has done its work, tell the power daemon to
  // re-suspend.
  std::move(suspend_readiness_cb_).Run();
  ClearDarkResumeState();
}

void ArcWakeLockBridge::FlushWakeLocksForTesting() {
  for (const auto& it : wake_lock_requesters_)
    it.second->FlushForTesting();
}

bool ArcWakeLockBridge::IsSuspendReadinessStateSetForTesting() const {
  return !suspend_readiness_cb_.is_null();
}

bool ArcWakeLockBridge::WakeLockHasObserversForTesting(
    device::mojom::WakeLockType type) {
  return GetWakeLockRequester(
             device::mojom::WakeLockType::kPreventAppSuspension)
      ->HasObservers();
}

void ArcWakeLockBridge::HandleDarkResumeWakeLockCheckTimeout() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(dark_resume_tasks_sequence_checker_);
  // Check if any wake locks are held at this point. If not, then it's assumed
  // the instance either acquired and released one or had no reason to acquire
  // one in the first place. If it wants to after this then too bad, tell the
  // power daemon to re-suspend and invalidate any other state associated with
  // dark resume.
  if (!GetWakeLockRequester(device::mojom::WakeLockType::kPreventAppSuspension)
           ->IsWakeLockHeld()) {
    DVLOG(1) << "Wake lock not held during check";
    std::move(suspend_readiness_cb_).Run();
    ClearDarkResumeState();
    return;
  }

  DVLOG(1) << "Wake lock held during check";
  // If a wake lock is held then register for a wake lock release
  // notification. As soon as it's released tell power daemon to re-suspend.
  // If the instance takes a long time then tell powerd daemon to re-suspend
  // after a hard timeout irrespective of wake locks held.
  GetWakeLockRequester(device::mojom::WakeLockType::kPreventAppSuspension)
      ->AddObserver(this);

  // Post task that will tell the power daemon to re-suspend after a dark
  // resume irrespective of any state. This is a last resort timeout to ensure
  // the device doesn't stay up indefinitely in dark resume.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcWakeLockBridge::HandleDarkResumeHardTimeout,
                     dark_resume_weak_ptr_factory_.GetWeakPtr()),
      kDarkResumeHardTimeout);
}

void ArcWakeLockBridge::HandleDarkResumeHardTimeout() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(dark_resume_tasks_sequence_checker_);
  // Enough is enough. Tell power daemon it's okay to suspend.
  DCHECK(suspend_readiness_cb_);
  std::move(suspend_readiness_cb_).Run();
  ClearDarkResumeState();
}

void ArcWakeLockBridge::ClearDarkResumeState() {
  DVLOG(1) << __func__;
  // Invalidate all other state associated with dark resume.
  suspend_readiness_cb_.Reset();
  GetWakeLockRequester(device::mojom::WakeLockType::kPreventAppSuspension)
      ->RemoveObserver(this);
  dark_resume_weak_ptr_factory_.InvalidateWeakPtrs();
}

ArcWakeLockBridge::WakeLockRequester* ArcWakeLockBridge::GetWakeLockRequester(
    device::mojom::WakeLockType type) {
  auto it = wake_lock_requesters_.find(type);
  if (it != wake_lock_requesters_.end())
    return it->second.get();

  service_manager::Connector* connector =
      connector_for_test_
          ? connector_for_test_
          : content::ServiceManagerConnection::GetForProcess()->GetConnector();
  DCHECK(connector);

  it = wake_lock_requesters_
           .emplace(type, std::make_unique<WakeLockRequester>(type, connector))
           .first;
  return it->second.get();
}

}  // namespace arc
