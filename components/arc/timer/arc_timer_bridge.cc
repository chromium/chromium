// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/timer/arc_timer_bridge.h"
#include "components/arc/timer/arc_timer_mojom_traits.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Tag to be used with the powerd timer API.
constexpr char kTag[] = "ARC";

mojom::ArcTimerResult ConvertBoolResultToMojo(bool result) {
  return result ? mojom::ArcTimerResult::SUCCESS
                : mojom::ArcTimerResult::FAILURE;
}

// Callback for powerd API called in |StartTimer|.
void OnStartTimer(mojom::TimerHost::StartTimerCallback callback, bool result) {
  std::move(callback).Run(ConvertBoolResultToMojo(result));
}

// Unwraps a mojo handle to a file descriptor on the system.
base::ScopedFD UnwrapScopedHandle(mojo::ScopedHandle handle) {
  base::PlatformFile platform_file;
  if (mojo::UnwrapPlatformFile(std::move(handle), &platform_file) !=
      MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed to unwrap mojo handle";
    return base::ScopedFD();
  }
  return base::ScopedFD(platform_file);
}

// Returns true iff |arc_timer_requests| contains duplicate clock id values.
bool ContainsDuplicateClocks(
    const std::vector<arc::mojom::CreateTimerRequestPtr>& arc_timer_requests) {
  std::set<clockid_t> seen_clock_ids;
  for (const auto& request : arc_timer_requests) {
    if (!seen_clock_ids.emplace(request->clock_id).second)
      return true;
  }
  return false;
}

// Singleton factory for ArcTimerBridge.
class ArcTimerBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcTimerBridge,
          ArcTimerBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcTimerBridgeFactory";

  static ArcTimerBridgeFactory* GetInstance() {
    return base::Singleton<ArcTimerBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcTimerBridgeFactory>;
  ArcTimerBridgeFactory() = default;
  ~ArcTimerBridgeFactory() override = default;
};

}  // namespace

// static
BrowserContextKeyedServiceFactory* ArcTimerBridge::GetFactory() {
  return ArcTimerBridgeFactory::GetInstance();
}

// static
ArcTimerBridge* ArcTimerBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcTimerBridgeFactory::GetForBrowserContext(context);
}

// static
ArcTimerBridge* ArcTimerBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcTimerBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcTimerBridge::ArcTimerBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service), binding_(this) {
  arc_bridge_service_->timer()->SetHost(this);
  arc_bridge_service_->timer()->AddObserver(this);
}

ArcTimerBridge::~ArcTimerBridge() {
  arc_bridge_service_->timer()->RemoveObserver(this);
  arc_bridge_service_->timer()->SetHost(nullptr);
}

void ArcTimerBridge::OnConnectionClosed() {
  DeleteArcTimers();
}

void ArcTimerBridge::CreateTimers(
    std::vector<arc::mojom::CreateTimerRequestPtr> arc_timer_requests,
    CreateTimersCallback callback) {
  // Duplicate clocks are not allowed.
  if (ContainsDuplicateClocks(arc_timer_requests)) {
    std::move(callback).Run(mojom::ArcTimerResult::FAILURE);
    return;
  }

  // Convert mojo arguments to D-Bus arguments required by powerd to create
  // timers.
  std::vector<std::pair<clockid_t, base::ScopedFD>> requests;
  std::vector<clockid_t> clock_ids;
  for (auto& request : arc_timer_requests) {
    clockid_t clock_id = request->clock_id;
    base::ScopedFD expiration_fd =
        UnwrapScopedHandle(std::move(request->expiration_fd));
    if (!expiration_fd.is_valid()) {
      LOG(ERROR) << "Unwrapped expiration fd is invalid for clock=" << clock_id;
      std::move(callback).Run(mojom::ArcTimerResult::FAILURE);
      return;
    }
    requests.emplace_back(clock_id, std::move(expiration_fd));
    clock_ids.emplace_back(clock_id);
  }
  chromeos::PowerManagerClient::Get()->CreateArcTimers(
      kTag, std::move(requests),
      base::BindOnce(&ArcTimerBridge::OnCreateArcTimers,
                     weak_ptr_factory_.GetWeakPtr(), std::move(clock_ids),
                     std::move(callback)));
}

void ArcTimerBridge::StartTimer(clockid_t clock_id,
                                base::TimeTicks absolute_expiration_time,
                                StartTimerCallback callback) {
  auto timer_id = GetTimerId(clock_id);
  if (!timer_id.has_value()) {
    LOG(ERROR) << "Timer for clock=" << clock_id << " not created";
    std::move(callback).Run(mojom::ArcTimerResult::FAILURE);
    return;
  }
  chromeos::PowerManagerClient::Get()->StartArcTimer(
      timer_id.value(), absolute_expiration_time,
      base::BindOnce(&OnStartTimer, std::move(callback)));
}

void ArcTimerBridge::DeleteArcTimers() {
  chromeos::PowerManagerClient::Get()->DeleteArcTimers(
      kTag, base::BindOnce(&ArcTimerBridge::OnDeleteArcTimers,
                           weak_ptr_factory_.GetWeakPtr()));
}

void ArcTimerBridge::OnDeleteArcTimers(bool result) {
  if (!result) {
    LOG(ERROR) << "Delete timers failed";
    return;
  }

  // If the delete call succeeded then delete any timer ids stored and make a
  // create timers call.
  DVLOG(1) << "Delete timers succeeded";
  timer_ids_.clear();
}

void ArcTimerBridge::OnCreateArcTimers(
    std::vector<clockid_t> clock_ids,
    CreateTimersCallback callback,
    base::Optional<std::vector<TimerId>> timer_ids) {
  // Any old timers associated with the same tag are always cleared by the API
  // regardless of the new timers being created successfully or not. Clear the
  // cached timer ids in that case.
  timer_ids_.clear();

  // The API returns a list of timer ids corresponding to each clock in
  // |clock_ids|.
  if (!timer_ids.has_value()) {
    LOG(ERROR) << "Create timers failed";
    std::move(callback).Run(mojom::ArcTimerResult::FAILURE);
    return;
  }

  std::vector<TimerId> result = timer_ids.value();
  if (result.size() != clock_ids.size()) {
    std::move(callback).Run(mojom::ArcTimerResult::FAILURE);
    return;
  }

  // Map clock id values to timer ids.
  auto timer_id_iter = result.begin();
  for (clockid_t clock_id : clock_ids) {
    DVLOG(1) << "Storing clock=" << clock_id << " timer id=" << *timer_id_iter;
    if (!timer_ids_.emplace(clock_id, *timer_id_iter).second) {
      // This should never happen as any collision should have been detected on
      // the powerd side and it should have returned an error.
      LOG(ERROR) << "Can't store clock=" << clock_id;
      timer_ids_.clear();
      std::move(callback).Run(mojom::ArcTimerResult::FAILURE);
      return;
    }
    timer_id_iter++;
  }
  std::move(callback).Run(mojom::ArcTimerResult::SUCCESS);
}

base::Optional<ArcTimerBridge::TimerId> ArcTimerBridge::GetTimerId(
    clockid_t clock_id) const {
  auto it = timer_ids_.find(clock_id);
  return (it == timer_ids_.end()) ? base::nullopt
                                  : base::make_optional<TimerId>(it->second);
}

}  // namespace arc
