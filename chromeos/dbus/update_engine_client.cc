// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/update_engine_client.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/dbus_switches.h"
#include "chromeos/dbus/util/version_loader.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kReleaseChannelCanary[] = "canary-channel";
const char kReleaseChannelDev[] = "dev-channel";
const char kReleaseChannelBeta[] = "beta-channel";
const char kReleaseChannelStable[] = "stable-channel";

// List of release channels ordered by stability.
const char* kReleaseChannelsList[] = {kReleaseChannelCanary, kReleaseChannelDev,
                                      kReleaseChannelBeta,
                                      kReleaseChannelStable};

// Delay between successive state transitions during AU.
const int kStateTransitionDefaultDelayMs = 3000;

// Delay between successive notifications about downloading progress
// during fake AU.
const int kStateTransitionDownloadingDelayMs = 250;

// Size of parts of a "new" image which are downloaded each
// |kStateTransitionDownloadingDelayMs| during fake AU.
const int64_t kDownloadSizeDelta = 1 << 19;

// Version number of the image being installed during fake AU.
const char kStubVersion[] = "1234.0.0.0";

// Returns UPDATE_STATUS_ERROR on error.
UpdateEngineClient::UpdateStatusOperation UpdateStatusFromString(
    const std::string& str) {
  VLOG(1) << "UpdateStatusFromString got " << str << " as input.";
  if (str == update_engine::kUpdateStatusIdle)
    return UpdateEngineClient::UPDATE_STATUS_IDLE;
  if (str == update_engine::kUpdateStatusCheckingForUpdate)
    return UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE;
  if (str == update_engine::kUpdateStatusUpdateAvailable)
    return UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE;
  if (str == update_engine::kUpdateStatusDownloading)
    return UpdateEngineClient::UPDATE_STATUS_DOWNLOADING;
  if (str == update_engine::kUpdateStatusVerifying)
    return UpdateEngineClient::UPDATE_STATUS_VERIFYING;
  if (str == update_engine::kUpdateStatusFinalizing)
    return UpdateEngineClient::UPDATE_STATUS_FINALIZING;
  if (str == update_engine::kUpdateStatusUpdatedNeedReboot)
    return UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT;
  if (str == update_engine::kUpdateStatusReportingErrorEvent)
    return UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT;
  if (str == update_engine::kUpdateStatusAttemptingRollback)
    return UpdateEngineClient::UPDATE_STATUS_ATTEMPTING_ROLLBACK;
  if (str == update_engine::kUpdateStatusNeedPermissionToUpdate)
    return UpdateEngineClient::UPDATE_STATUS_NEED_PERMISSION_TO_UPDATE;
  return UpdateEngineClient::UPDATE_STATUS_ERROR;
}

bool IsValidChannel(const std::string& channel) {
  return channel == kReleaseChannelDev || channel == kReleaseChannelBeta ||
         channel == kReleaseChannelStable;
}

}  // namespace

// The UpdateEngineClient implementation used in production.
class UpdateEngineClientImpl : public UpdateEngineClient {
 public:
  UpdateEngineClientImpl()
      : update_engine_proxy_(NULL), last_status_(), weak_ptr_factory_(this) {}

  ~UpdateEngineClientImpl() override = default;

  // UpdateEngineClient implementation:
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(const Observer* observer) const override {
    return observers_.HasObserver(observer);
  }

  void RequestUpdateCheck(const UpdateCheckCallback& callback) override {
    if (!service_available_) {
      // TODO(alemate): we probably need to remember callbacks only.
      // When service becomes available, we can do a single request,
      // and trigger all callbacks with the same return value.
      pending_tasks_.push_back(
          base::Bind(&UpdateEngineClientImpl::RequestUpdateCheck,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }
    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kAttemptUpdate);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("");  // Unused.
    writer.AppendString("");  // Unused.

    VLOG(1) << "Requesting an update check";
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnRequestUpdateCheck,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RebootAfterUpdate() override {
    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kRebootIfNeeded);

    VLOG(1) << "Requesting a reboot";
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnRebootAfterUpdate,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void Rollback() override {
    VLOG(1) << "Requesting a rollback";
     dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kAttemptRollback);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(true /* powerwash */);

    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnRollback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void CanRollbackCheck(const RollbackCheckCallback& callback) override {
    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kCanRollback);

    VLOG(1) << "Requesting to get rollback availability status";
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnCanRollbackCheck,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  Status GetLastStatus() override { return last_status_; }

  void SetChannel(const std::string& target_channel,
                  bool is_powerwash_allowed) override {
    if (!IsValidChannel(target_channel)) {
      LOG(ERROR) << "Invalid channel name: " << target_channel;
      return;
    }

    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kSetChannel);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(target_channel);
    writer.AppendBool(is_powerwash_allowed);

    VLOG(1) << "Requesting to set channel: "
            << "target_channel=" << target_channel << ", "
            << "is_powerwash_allowed=" << is_powerwash_allowed;
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnSetChannel,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void GetChannel(bool get_current_channel,
                  const GetChannelCallback& callback) override {
    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kGetChannel);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(get_current_channel);

    VLOG(1) << "Requesting to get channel, get_current_channel="
            << get_current_channel;
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnGetChannel,
                       weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void GetEolStatus(GetEolStatusCallback callback) override {
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kGetEolStatus);

    VLOG(1) << "Requesting to get end of life status";
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnGetEolStatus,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetUpdateOverCellularPermission(bool allowed,
                                       const base::Closure& callback) override {
    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kSetUpdateOverCellularPermission);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(allowed);

    VLOG(1) << "Requesting UpdateEngine to " << (allowed ? "allow" : "prohibit")
            << " updates over cellular.";

    return update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &UpdateEngineClientImpl::OnSetUpdateOverCellularPermission,
            weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void SetUpdateOverCellularOneTimePermission(
      const std::string& update_version,
      int64_t update_size,
      const UpdateOverCellularOneTimePermissionCallback& callback) override {
    // TODO(weidongg): Change 'kSetUpdateOverCellularTarget' to
    // 'kSetUpdateOverCellularOneTimePermission'
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kSetUpdateOverCellularTarget);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(update_version);
    writer.AppendInt64(update_size);

    VLOG(1) << "Requesting UpdateEngine to allow updates over cellular "
            << "to target version: \"" << update_version << "\" "
            << "target_size: " << update_size;

    return update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &UpdateEngineClientImpl::OnSetUpdateOverCellularOneTimePermission,
            weak_ptr_factory_.GetWeakPtr(), callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    update_engine_proxy_ = bus->GetObjectProxy(
        update_engine::kUpdateEngineServiceName,
        dbus::ObjectPath(update_engine::kUpdateEngineServicePath));
    update_engine_proxy_->ConnectToSignal(
        update_engine::kUpdateEngineInterface, update_engine::kStatusUpdate,
        base::Bind(&UpdateEngineClientImpl::StatusUpdateReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&UpdateEngineClientImpl::StatusUpdateConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    update_engine_proxy_->WaitForServiceToBeAvailable(
        base::BindOnce(&UpdateEngineClientImpl::OnServiceInitiallyAvailable,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnServiceInitiallyAvailable(bool service_is_available) {
    if (service_is_available) {
      service_available_ = true;
      std::vector<base::Closure> callbacks;
      callbacks.swap(pending_tasks_);
      for (const auto& callback : callbacks) {
        callback.Run();
      }

      // Get update engine status for the initial status. Update engine won't
      // send StatusUpdate signal unless there is a status change. If chrome
      // crashes after UPDATE_STATUS_UPDATED_NEED_REBOOT status is set,
      // restarted chrome would not get this status. See crbug.com/154104.
      GetUpdateEngineStatus();
    } else {
      LOG(ERROR) << "Failed to wait for D-Bus service to become available";
      pending_tasks_.clear();
    }
  }

  void GetUpdateEngineStatus() {
    dbus::MethodCall method_call(
        update_engine::kUpdateEngineInterface,
        update_engine::kGetStatus);
    update_engine_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnGetStatus,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&UpdateEngineClientImpl::OnGetStatusError,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Called when a response for RequestUpdateCheck() is received.
  void OnRequestUpdateCheck(const UpdateCheckCallback& callback,
                            dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request update check";
      callback.Run(UPDATE_RESULT_FAILED);
      return;
    }
    callback.Run(UPDATE_RESULT_SUCCESS);
  }

  // Called when a response for RebootAfterUpdate() is received.
  void OnRebootAfterUpdate(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request rebooting after update";
      return;
    }
  }

  // Called when a response for Rollback() is received.
  void OnRollback(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to rollback";
      return;
    }
  }

  // Called when a response for CanRollbackCheck() is received.
  void OnCanRollbackCheck(const RollbackCheckCallback& callback,
                          dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request rollback availability status";
      callback.Run(false);
      return;
    }
    dbus::MessageReader reader(response);
    bool can_rollback;
    if (!reader.PopBool(&can_rollback)) {
      LOG(ERROR) << "Incorrect response: " << response->ToString();
      callback.Run(false);
      return;
    }
    VLOG(1) << "Rollback availability status received: " << can_rollback;
    callback.Run(can_rollback);
  }

  // Called when a response for GetStatus is received.
  void OnGetStatus(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to get response for GetStatus request.";
      return;
    }

    dbus::MessageReader reader(response);
    std::string current_operation;
    Status status;
    if (!(reader.PopInt64(&status.last_checked_time) &&
          reader.PopDouble(&status.download_progress) &&
          reader.PopString(&current_operation) &&
          reader.PopString(&status.new_version) &&
          reader.PopInt64(&status.new_size))) {
      LOG(ERROR) << "GetStatus had incorrect response: "
                 << response->ToString();
      return;
    }
    status.status = UpdateStatusFromString(current_operation);
    // TODO(hunyadym, https://crbug.com/864672): Add a new DBus call to
    // determine this based on the Omaha response, and not version comparison.
    const std::string current_version =
        version_loader::GetVersion(version_loader::VERSION_SHORT);
    status.is_rollback =
        version_loader::IsRollback(current_version, status.new_version);
    if (status.is_rollback) {
      LOG(WARNING) << "New image is a rollback from " << current_version
                   << " to " << status.new_version << ".";
    }

    last_status_ = status;
    for (auto& observer : observers_)
      observer.UpdateStatusChanged(status);
  }

  // Called when GetStatus call failed.
  void OnGetStatusError(dbus::ErrorResponse* error) {
    LOG(ERROR) << "GetStatus request failed with error: "
               << (error ? error->ToString() : "");
  }

  // Called when a response for SetReleaseChannel() is received.
  void OnSetChannel(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request setting channel";
      return;
    }
    VLOG(1) << "Succeeded to set channel";
  }

  // Called when a response for GetChannel() is received.
  void OnGetChannel(const GetChannelCallback& callback,
                    dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request getting channel";
      callback.Run("");
      return;
    }
    dbus::MessageReader reader(response);
    std::string channel;
    if (!reader.PopString(&channel)) {
      LOG(ERROR) << "Incorrect response: " << response->ToString();
      callback.Run("");
      return;
    }
    VLOG(1) << "The channel received: " << channel;
    callback.Run(channel);
  }

  // Called when a response for GetEolStatus() is received.
  void OnGetEolStatus(GetEolStatusCallback callback, dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request getting eol status";
      std::move(callback).Run(update_engine::EndOfLifeStatus::kSupported);
      return;
    }
    dbus::MessageReader reader(response);
    int status;
    if (!reader.PopInt32(&status)) {
      LOG(ERROR) << "Incorrect response: " << response->ToString();
      std::move(callback).Run(update_engine::EndOfLifeStatus::kSupported);
      return;
    }

    // Validate the value of status
    if (status > update_engine::EndOfLifeStatus::kEol ||
        status < update_engine::EndOfLifeStatus::kSupported) {
      LOG(ERROR) << "Incorrect status value: " << status;
      std::move(callback).Run(update_engine::EndOfLifeStatus::kSupported);
      return;
    }

    VLOG(1) << "Eol status received: " << status;
    std::move(callback).Run(
        static_cast<update_engine::EndOfLifeStatus>(status));
  }

  // Called when a response for SetUpdateOverCellularPermission() is received.
  void OnSetUpdateOverCellularPermission(const base::Closure& callback,
                                         dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << update_engine::kSetUpdateOverCellularPermission
                 << " call failed";
    }

    // Callback should run anyway, regardless of whether DBus call to enable
    // update over cellular succeeded or failed.
    callback.Run();
  }

  // Called when a response for SetUpdateOverCellularOneTimePermission() is
  // received.
  void OnSetUpdateOverCellularOneTimePermission(
      const UpdateOverCellularOneTimePermissionCallback& callback,
      dbus::Response* response) {
    bool success = true;
    if (!response) {
      success = false;
      LOG(ERROR) << update_engine::kSetUpdateOverCellularTarget
                 << " call failed";
    }

    if (success) {
      for (auto& observer : observers_) {
        observer.OnUpdateOverCellularOneTimePermissionGranted();
      }
    }

    callback.Run(success);
  }

  // Called when a status update signal is received.
  void StatusUpdateReceived(dbus::Signal* signal) {
    VLOG(1) << "Status update signal received: " << signal->ToString();
    dbus::MessageReader reader(signal);
    int64_t last_checked_time = 0;
    double progress = 0.0;
    std::string current_operation;
    std::string new_version;
    int64_t new_size = 0;
    if (!(reader.PopInt64(&last_checked_time) &&
          reader.PopDouble(&progress) &&
          reader.PopString(&current_operation) &&
          reader.PopString(&new_version) &&
          reader.PopInt64(&new_size))) {
      LOG(ERROR) << "Status changed signal had incorrect parameters: "
                 << signal->ToString();
      return;
    }
    Status status;
    status.last_checked_time = last_checked_time;
    status.download_progress = progress;
    status.status = UpdateStatusFromString(current_operation);
    status.new_version = new_version;
    // TODO(hunyadym, https://crbug.com/864672): Add a new DBus call to
    // determine this based on the Omaha response, and not version comparison.
    const std::string current_version =
        version_loader::GetVersion(version_loader::VERSION_SHORT);
    status.is_rollback =
        version_loader::IsRollback(current_version, status.new_version);
    if (status.is_rollback) {
      LOG(WARNING) << "New image is a rollback from " << current_version
                   << " to " << new_version << ".";
    }

    status.new_size = new_size;

    last_status_ = status;
    for (auto& observer : observers_)
      observer.UpdateStatusChanged(status);
  }

  // Called when the status update signal is initially connected.
  void StatusUpdateConnected(const std::string& interface_name,
                             const std::string& signal_name,
                             bool success) {
    LOG_IF(WARNING, !success)
        << "Failed to connect to status updated signal.";
  }

  dbus::ObjectProxy* update_engine_proxy_;
  base::ObserverList<Observer>::Unchecked observers_;
  Status last_status_;

  // True after update_engine's D-Bus service has become available.
  bool service_available_ = false;

  // This is a list of postponed calls to update engine to be called
  // after it becomes available.
  std::vector<base::Closure> pending_tasks_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UpdateEngineClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpdateEngineClientImpl);
};

// The UpdateEngineClient implementation used on Linux desktop,
// which does nothing.
class UpdateEngineClientStubImpl : public UpdateEngineClient {
 public:
  UpdateEngineClientStubImpl()
      : current_channel_(kReleaseChannelBeta),
        target_channel_(kReleaseChannelBeta),
        weak_factory_(this) {}

  // UpdateEngineClient implementation:
  void Init(dbus::Bus* bus) override {}

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(const Observer* observer) const override {
    return observers_.HasObserver(observer);
  }

  void RequestUpdateCheck(const UpdateCheckCallback& callback) override {
    if (last_status_.status != UPDATE_STATUS_IDLE) {
      callback.Run(UPDATE_RESULT_FAILED);
      return;
    }
    callback.Run(UPDATE_RESULT_SUCCESS);
    last_status_.status = UPDATE_STATUS_CHECKING_FOR_UPDATE;
    last_status_.download_progress = 0.0;
    last_status_.last_checked_time = 0;
    last_status_.new_version = "0.0.0.0";
    last_status_.new_size = 0;
    last_status_.is_rollback = false;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UpdateEngineClientStubImpl::StateTransition,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kStateTransitionDefaultDelayMs));
  }

  void RebootAfterUpdate() override {}

  void Rollback() override {}

  void CanRollbackCheck(const RollbackCheckCallback& callback) override {
    callback.Run(true);
  }

  Status GetLastStatus() override { return last_status_; }

  void SetChannel(const std::string& target_channel,
                  bool is_powerwash_allowed) override {
    VLOG(1) << "Requesting to set channel: "
            << "target_channel=" << target_channel << ", "
            << "is_powerwash_allowed=" << is_powerwash_allowed;
    target_channel_ = target_channel;
  }
  void GetChannel(bool get_current_channel,
                  const GetChannelCallback& callback) override {
    VLOG(1) << "Requesting to get channel, get_current_channel="
            << get_current_channel;
    if (get_current_channel)
      callback.Run(current_channel_);
    else
      callback.Run(target_channel_);
  }

  void GetEolStatus(GetEolStatusCallback callback) override {
    std::move(callback).Run(update_engine::EndOfLifeStatus::kSupported);
  }

  void SetUpdateOverCellularPermission(bool allowed,
                                       const base::Closure& callback) override {
    callback.Run();
  }

  void SetUpdateOverCellularOneTimePermission(
      const std::string& update_version,
      int64_t update_size,
      const UpdateOverCellularOneTimePermissionCallback& callback) override {}

 private:
  void StateTransition() {
    UpdateStatusOperation next_status = UPDATE_STATUS_ERROR;
    int delay_ms = kStateTransitionDefaultDelayMs;
    switch (last_status_.status) {
      case UPDATE_STATUS_ERROR:
      case UPDATE_STATUS_IDLE:
      case UPDATE_STATUS_UPDATED_NEED_REBOOT:
      case UPDATE_STATUS_REPORTING_ERROR_EVENT:
      case UPDATE_STATUS_ATTEMPTING_ROLLBACK:
      case UPDATE_STATUS_NEED_PERMISSION_TO_UPDATE:
        return;
      case UPDATE_STATUS_CHECKING_FOR_UPDATE:
        next_status = UPDATE_STATUS_UPDATE_AVAILABLE;
        break;
      case UPDATE_STATUS_UPDATE_AVAILABLE:
        next_status = UPDATE_STATUS_DOWNLOADING;
        break;
      case UPDATE_STATUS_DOWNLOADING:
        if (last_status_.download_progress >= 1.0) {
          next_status = UPDATE_STATUS_VERIFYING;
        } else {
          next_status = UPDATE_STATUS_DOWNLOADING;
          last_status_.download_progress += 0.01;
          last_status_.new_version = kStubVersion;
          last_status_.new_size = kDownloadSizeDelta;
          delay_ms = kStateTransitionDownloadingDelayMs;
        }
        break;
      case UPDATE_STATUS_VERIFYING:
        next_status = UPDATE_STATUS_FINALIZING;
        break;
      case UPDATE_STATUS_FINALIZING:
        next_status = UPDATE_STATUS_UPDATED_NEED_REBOOT;
        break;
    }
    last_status_.status = next_status;
    for (auto& observer : observers_)
      observer.UpdateStatusChanged(last_status_);
    if (last_status_.status != UPDATE_STATUS_IDLE) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&UpdateEngineClientStubImpl::StateTransition,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(delay_ms));
    }
  }

  base::ObserverList<Observer>::Unchecked observers_;

  std::string current_channel_;
  std::string target_channel_;

  Status last_status_;

  base::WeakPtrFactory<UpdateEngineClientStubImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpdateEngineClientStubImpl);
};

UpdateEngineClient::UpdateEngineClient() = default;

UpdateEngineClient::~UpdateEngineClient() = default;

// static
UpdateEngineClient* UpdateEngineClient::Create(
    DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return new UpdateEngineClientImpl();
  DCHECK_EQ(FAKE_DBUS_CLIENT_IMPLEMENTATION, type);
  return new UpdateEngineClientStubImpl();
}

// static
bool UpdateEngineClient::IsTargetChannelMoreStable(
    const std::string& current_channel,
    const std::string& target_channel) {
  const char** cix = std::find(
      kReleaseChannelsList,
      kReleaseChannelsList + arraysize(kReleaseChannelsList), current_channel);
  const char** tix = std::find(
      kReleaseChannelsList,
      kReleaseChannelsList + arraysize(kReleaseChannelsList), target_channel);
  return tix > cix;
}

}  // namespace chromeos
