// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/resourced/resourced_client.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_metrics.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/resource_manager/resource_manager.pb.h"
#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/resource_manager/dbus-constants.h"

namespace ash {
namespace {

// Resource manager D-Bus method calls are all simple operations and should
// not take more than 1 second.
constexpr int kResourcedDBusTimeoutMilliseconds = 1000;

ResourcedClient* g_instance = nullptr;

class ResourcedClientImpl : public ResourcedClient {
 public:
  ResourcedClientImpl();
  ~ResourcedClientImpl() override = default;
  ResourcedClientImpl(const ResourcedClientImpl&) = delete;
  ResourcedClientImpl& operator=(const ResourcedClientImpl&) = delete;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        resource_manager::kResourceManagerServiceName,
        dbus::ObjectPath(resource_manager::kResourceManagerServicePath));
    proxy_->ConnectToSignal(
        resource_manager::kResourceManagerInterface,
        resource_manager::kMemoryPressureChrome,
        base::BindRepeating(&ResourcedClientImpl::MemoryPressureReceived,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&ResourcedClientImpl::MemoryPressureConnected,
                       weak_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        resource_manager::kResourceManagerInterface,
        resource_manager::kMemoryPressureArcContainer,
        base::BindRepeating(
            &ResourcedClientImpl::MemoryPressureArcContainerReceived,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(&ResourcedClientImpl::MemoryPressureConnected,
                       weak_factory_.GetWeakPtr()));
  }

  // ResourcedClient interface.
  void SetGameModeWithTimeout(
      GameMode game_mode,
      uint32_t refresh_seconds,
      chromeos::DBusMethodCallback<GameMode> callback) override;

  void SetMemoryMargins(MemoryMargins margins) override;

  void ReportBrowserProcesses(Component component,
                              const std::vector<Process>& processes) override;

  void SetProcessState(base::ProcessId process_id,
                       resource_manager::ProcessState state,
                       SetQoSStateCallback callback) override;

  void SetThreadState(base::ProcessId process_id,
                      base::PlatformThreadId thread_id,
                      resource_manager::ThreadState state,
                      SetQoSStateCallback callback) override;

  void AddObserver(Observer* observer) override;

  void RemoveObserver(Observer* observer) override;

  void AddArcContainerObserver(ArcContainerObserver* observer) override;

  void RemoveArcContainerObserver(ArcContainerObserver* observer) override;

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

 private:
  // D-Bus response handlers.
  void HandleSetGameModeWithTimeoutResponse(
      chromeos::DBusMethodCallback<GameMode> callback,
      dbus::Response* response);

  // D-Bus signal handlers.
  void MemoryPressureReceived(dbus::Signal* signal);
  void MemoryPressureConnected(const std::string& interface_name,
                               const std::string& signal_name,
                               bool success);

  void MemoryPressureArcContainerReceived(dbus::Signal* signal);

  void HandleSetProcessStateResponse(base::ProcessId process_id,
                                     SetQoSStateCallback callback,
                                     dbus::Response* response,
                                     dbus::ErrorResponse* error);

  void HandleSetThreadStateResponse(base::PlatformThreadId thread_id,
                                    SetQoSStateCallback callback,
                                    dbus::Response* response,
                                    dbus::ErrorResponse* error);

  // Member variables.

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // Caches the total memory for reclaim_target_kb sanity check. The default
  // value is 32 GiB in case reading total memory failed.
  uint64_t total_memory_kb_ = 32 * 1024 * 1024;

  // A list of observers that are listening on state changes, etc.
  base::ObserverList<Observer> observers_;

  // A list of observers listening for ARC container memory pressure signals.
  base::ObserverList<ArcContainerObserver> arc_container_observers_;

  base::WeakPtrFactory<ResourcedClientImpl> weak_factory_{this};
};

ResourcedClientImpl::ResourcedClientImpl() {
  base::SystemMemoryInfoKB info;
  if (base::GetSystemMemoryInfo(&info)) {
    total_memory_kb_ = static_cast<uint64_t>(info.total);
  } else {
    PLOG(ERROR) << "Error reading total memory.";
  }
}

void ResourcedClientImpl::MemoryPressureReceived(dbus::Signal* signal) {
  dbus::MessageReader signal_reader(signal);

  memory_pressure::ReclaimTarget reclaim_target;
  uint8_t pressure_level_byte;
  PressureLevel pressure_level;

  if (!signal_reader.PopByte(&pressure_level_byte) ||
      !signal_reader.PopUint64(&reclaim_target.target_kb)) {
    LOG(ERROR) << "Error reading signal from resourced: " << signal->ToString();
    return;
  }

  int64_t signal_origin_timestamp_ms = -1;
  // The signal origin timestamp may not be included by resourced, and if it is,
  // it might be an invalid value.
  if (signal_reader.PopInt64(&signal_origin_timestamp_ms) &&
      signal_origin_timestamp_ms > 0) {
    // Signal origin timestamp is received as a ms value from CLOCK_MONOTONIC.
    reclaim_target.origin_time =
        base::TimeTicks::FromUptimeMillis(signal_origin_timestamp_ms);
  }

  uint8_t discard_type;
  if (signal_reader.PopByte(&discard_type)) {
    if (discard_type == resource_manager::DiscardType::UNPROTECTED) {
      reclaim_target.discard_protected = false;
    } else if (discard_type == resource_manager::DiscardType::PROTECTED) {
      reclaim_target.discard_protected = true;
    } else {
      LOG(ERROR) << "Unknown discard type: " << discard_type;
    }
  }

  if (pressure_level_byte == resource_manager::PressureLevelChrome::NONE) {
    pressure_level = PressureLevel::NONE;
  } else if (pressure_level_byte ==
             resource_manager::PressureLevelChrome::MODERATE) {
    pressure_level = PressureLevel::MODERATE;
  } else if (pressure_level_byte ==
             resource_manager::PressureLevelChrome::CRITICAL) {
    pressure_level = PressureLevel::CRITICAL;
  } else {
    LOG(ERROR) << "Unknown memory pressure level: " << pressure_level_byte;
    return;
  }

  if (reclaim_target.target_kb > total_memory_kb_) {
    LOG(ERROR) << "reclaim_target_kb is too large: "
               << reclaim_target.target_kb;
    return;
  }

  for (auto& observer : observers_) {
    observer.OnMemoryPressure(pressure_level, reclaim_target);
  }
}

void ResourcedClientImpl::MemoryPressureArcContainerReceived(
    dbus::Signal* signal) {
  dbus::MessageReader signal_reader(signal);

  uint8_t pressure_level_byte;
  PressureLevelArcContainer pressure_level;
  uint64_t reclaim_target_kb;

  if (!signal_reader.PopByte(&pressure_level_byte) ||
      !signal_reader.PopUint64(&reclaim_target_kb)) {
    LOG(ERROR) << "Error reading signal from resourced: " << signal->ToString();
    return;
  }
  switch (static_cast<resource_manager::PressureLevelArcContainer>(
      pressure_level_byte)) {
    case resource_manager::PressureLevelArcContainer::NONE:
      pressure_level = PressureLevelArcContainer::kNone;
      break;

    case resource_manager::PressureLevelArcContainer::CACHED:
      pressure_level = PressureLevelArcContainer::kCached;
      break;

    case resource_manager::PressureLevelArcContainer::PERCEPTIBLE:
      pressure_level = PressureLevelArcContainer::kPerceptible;
      break;

    case resource_manager::PressureLevelArcContainer::FOREGROUND:
      pressure_level = PressureLevelArcContainer::kForeground;
      break;

    default:
      LOG(ERROR) << "Unknown memory pressure level: " << pressure_level_byte;
      return;
  }

  if (reclaim_target_kb > total_memory_kb_) {
    LOG(ERROR) << "reclaim_target_kb is too large: " << reclaim_target_kb;
    return;
  }

  for (auto& observer : arc_container_observers_) {
    observer.OnMemoryPressure(pressure_level, reclaim_target_kb);
  }
}

void ResourcedClientImpl::HandleSetProcessStateResponse(
    base::ProcessId process_id,
    SetQoSStateCallback callback,
    dbus::Response* response,
    dbus::ErrorResponse* error) {
  dbus::DBusResult result = dbus::DBusResult::kSuccess;
  if (response == nullptr) {
    result = dbus::GetResult(error);
  }
  std::move(callback).Run(result);
}

void ResourcedClientImpl::HandleSetThreadStateResponse(
    base::PlatformThreadId thread_id,
    SetQoSStateCallback callback,
    dbus::Response* response,
    dbus::ErrorResponse* error) {
  dbus::DBusResult result = dbus::DBusResult::kSuccess;
  if (response == nullptr) {
    result = dbus::GetResult(error);
  }
  std::move(callback).Run(result);
}

void ResourcedClientImpl::MemoryPressureConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  PLOG_IF(ERROR, !success) << "Failed to connect to signal: " << signal_name;
}

// Response will be true if game mode was on previously, false otherwise.
void ResourcedClientImpl::HandleSetGameModeWithTimeoutResponse(
    chromeos::DBusMethodCallback<GameMode> callback,
    dbus::Response* response) {
  dbus::MessageReader reader(response);
  uint8_t previous;
  if (!reader.PopByte(&previous)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(static_cast<GameMode>(previous));
}

void ResourcedClientImpl::SetGameModeWithTimeout(
    GameMode game_mode,
    uint32_t refresh_seconds,
    chromeos::DBusMethodCallback<GameMode> callback) {
  dbus::MethodCall method_call(resource_manager::kResourceManagerInterface,
                               resource_manager::kSetGameModeWithTimeoutMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendByte(static_cast<uint8_t>(game_mode));
  writer.AppendUint32(refresh_seconds);

  proxy_->CallMethod(
      &method_call, kResourcedDBusTimeoutMilliseconds,
      base::BindOnce(&ResourcedClientImpl::HandleSetGameModeWithTimeoutResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ResourcedClientImpl::SetMemoryMargins(MemoryMargins margins) {
  resource_manager::MemoryMargins request;
  request.set_moderate_bps(margins.moderate_bps);
  request.set_critical_bps(margins.critical_bps);
  request.set_critical_protected_bps(margins.critical_protected_bps);

  dbus::MethodCall method_call(resource_manager::kResourceManagerInterface,
                               resource_manager::kSetMemoryMarginsMethod);
  if (!dbus::MessageWriter(&method_call).AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Error serializing "
               << resource_manager::kSetMemoryMarginsMethod << " request";
    return;
  }

  proxy_->CallMethod(&method_call, kResourcedDBusTimeoutMilliseconds,
                     base::DoNothing());
}

void ResourcedClientImpl::ReportBrowserProcesses(
    Component component,
    const std::vector<Process>& processes) {
  resource_manager::ReportBrowserProcesses request;

  if (component == ResourcedClient::Component::kAsh) {
    request.set_browser_type(resource_manager::BrowserType::ASH);
  } else if (component == ResourcedClient::Component::kLacros) {
    request.set_browser_type(resource_manager::BrowserType::LACROS);
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  for (auto it = processes.begin(); it != processes.end(); ++it) {
    auto* process = request.add_processes();
    process->set_pid(it->pid);
    process->set_protected_(it->is_protected);
    process->set_visible(it->is_visible);
    process->set_focused(it->is_focused);
    process->set_last_visible_ms(
        it->last_visible.since_origin().InMilliseconds());
  }

  dbus::MethodCall method_call(resource_manager::kResourceManagerInterface,
                               resource_manager::kReportBrowserProcessesMethod);
  if (!dbus::MessageWriter(&method_call).AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Error serializing "
               << resource_manager::kReportBrowserProcessesMethod << " request";
    return;
  }

  proxy_->CallMethod(&method_call, kResourcedDBusTimeoutMilliseconds,
                     base::DoNothing());
}

void ResourcedClientImpl::SetProcessState(base::ProcessId process_id,
                                          resource_manager::ProcessState state,
                                          SetQoSStateCallback callback) {
  dbus::MethodCall method_call(resource_manager::kResourceManagerInterface,
                               resource_manager::kSetProcessStateMethod);
  dbus::MessageWriter writer(&method_call);

  writer.AppendUint32(process_id);
  writer.AppendByte(static_cast<uint8_t>(state));

  proxy_->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ResourcedClientImpl::HandleSetProcessStateResponse,
                     weak_factory_.GetWeakPtr(), process_id,
                     std::move(callback)));
}

void ResourcedClientImpl::SetThreadState(base::ProcessId process_id,
                                         base::PlatformThreadId thread_id,
                                         resource_manager::ThreadState state,
                                         SetQoSStateCallback callback) {
  dbus::MethodCall method_call(resource_manager::kResourceManagerInterface,
                               resource_manager::kSetThreadStateMethod);
  dbus::MessageWriter writer(&method_call);

  writer.AppendUint32(process_id);
  writer.AppendUint32(thread_id);
  writer.AppendByte(static_cast<uint8_t>(state));

  proxy_->CallMethodWithErrorResponse(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&ResourcedClientImpl::HandleSetThreadStateResponse,
                     weak_factory_.GetWeakPtr(), thread_id,
                     std::move(callback)));
}

void ResourcedClientImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ResourcedClientImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ResourcedClientImpl::AddArcContainerObserver(
    ArcContainerObserver* observer) {
  arc_container_observers_.AddObserver(observer);
}

void ResourcedClientImpl::RemoveArcContainerObserver(
    ArcContainerObserver* observer) {
  arc_container_observers_.RemoveObserver(observer);
}

void ResourcedClientImpl::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  proxy_->WaitForServiceToBeAvailable(std::move(callback));
}

}  // namespace

ResourcedClient::ResourcedClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ResourcedClient::~ResourcedClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ResourcedClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ResourcedClientImpl())->Init(bus);
}

// static
FakeResourcedClient* ResourcedClient::InitializeFake() {
  return new FakeResourcedClient();
}

// static
void ResourcedClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
ResourcedClient* ResourcedClient::Get() {
  return g_instance;
}

}  // namespace ash
