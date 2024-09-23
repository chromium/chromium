// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/version/version_loader.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

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

UpdateEngineClient* g_instance = nullptr;

// The UpdateEngineClient implementation used in production.
class UpdateEngineClientImpl : public UpdateEngineClient {
 public:
  UpdateEngineClientImpl() : update_engine_proxy_(nullptr), last_status_() {}

  UpdateEngineClientImpl(const UpdateEngineClientImpl&) = delete;
  UpdateEngineClientImpl& operator=(const UpdateEngineClientImpl&) = delete;

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

  void RequestUpdateCheck(UpdateCheckCallback callback) override {
    update_engine::UpdateParams update_params;
    update_params.set_app_version("");
    update_params.set_omaha_url("");
    // Default is interactive as |true|, but explicitly set here.
    update_params.mutable_update_flags()->set_non_interactive(false);

    RequestUpdateCheckInternal(std::move(callback), std::move(update_params));
  }

  void RequestUpdateCheckWithoutApplying(
      UpdateCheckCallback callback) override {
    update_engine::UpdateParams update_params;
    update_params.set_app_version("");
    update_params.set_omaha_url("");
    // Default is interactive as |true|, but explicitly set here.
    update_params.mutable_update_flags()->set_non_interactive(false);
    update_params.set_skip_applying(true);

    RequestUpdateCheckInternal(std::move(callback), std::move(update_params));
  }

  void RebootAfterUpdate() override {
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kRebootIfNeeded);

    VLOG(1) << "Requesting a reboot";
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnRebootAfterUpdate,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void Rollback() override {
    VLOG(1) << "Requesting a rollback";
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kAttemptRollback);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(true /* powerwash */);

    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnRollback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void CanRollbackCheck(RollbackCheckCallback callback) override {
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kCanRollback);

    VLOG(1) << "Requesting to get rollback availability status";
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnCanRollbackCheck,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  update_engine::StatusResult GetLastStatus() override { return last_status_; }

  void SetChannel(const std::string& target_channel,
                  bool is_powerwash_allowed) override {
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
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
                  GetChannelCallback callback) override {
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kGetChannel);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(get_current_channel);

    VLOG(1) << "Requesting to get channel, get_current_channel="
            << get_current_channel;
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnGetChannel,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetEolInfo(GetEolInfoCallback callback) override {
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kGetStatusAdvanced);

    VLOG(1) << "Requesting to get end of life status";
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnGetEolInfo,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetUpdateOverCellularPermission(bool allowed,
                                       base::OnceClosure callback) override {
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
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetUpdateOverCellularOneTimePermission(
      const std::string& update_version,
      int64_t update_size,
      UpdateOverCellularOneTimePermissionCallback callback) override {
    // TODO(https://crbug.com/927439): Change 'kSetUpdateOverCellularTarget' to
    // 'kSetUpdateOverCellularOneTimePermission'
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kSetUpdateOverCellularTarget);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(update_version);
    writer.AppendInt64(update_size);

    VLOG(1) << "Requesting UpdateEngine to allow updates over cellular "
            << "to target version: \"" << update_version << "\" "
            << "target_size: " << update_size;

    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &UpdateEngineClientImpl::OnSetUpdateOverCellularOneTimePermission,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ToggleFeature(const std::string& feature, bool enable) override {
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kToggleFeature);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(feature);
    writer.AppendBool(enable);
    VLOG(1) << "Requesting UpdateEngine to " << (enable ? "enable" : "disable")
            << " feature " << feature;

    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnToggleFeature,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void IsFeatureEnabled(const std::string& feature,
                        IsFeatureEnabledCallback callback) override {
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kIsFeatureEnabled);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(feature);

    VLOG(1) << "Requesting UpdateEngine to get feature " << feature;

    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnIsFeatureEnabled,
                       weak_ptr_factory_.GetWeakPtr(), feature,
                       std::move(callback)));
  }

  void ApplyDeferredUpdate(bool shutdown_after_update,
                           base::OnceClosure failure_callback) override {
    update_engine::ApplyUpdateConfig config;
    config.set_done_action(shutdown_after_update
                               ? update_engine::UpdateDoneAction::SHUTDOWN
                               : update_engine::UpdateDoneAction::REBOOT);
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kApplyDeferredUpdateAdvanced);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(config)) {
      LOG(ERROR) << "Failed to encode ApplyUpdateConfig protobuf.";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(failure_callback));
      return;
    }

    VLOG(1) << "Requesting UpdateEngine to apply deferred update.";

    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnApplyDeferredUpdate,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(failure_callback)));
  }

  void Init(dbus::Bus* bus) override {
    update_engine_proxy_ = bus->GetObjectProxy(
        update_engine::kUpdateEngineServiceName,
        dbus::ObjectPath(update_engine::kUpdateEngineServicePath));
    update_engine_proxy_->ConnectToSignal(
        update_engine::kUpdateEngineInterface,
        update_engine::kStatusUpdateAdvanced,
        base::BindRepeating(&UpdateEngineClientImpl::StatusUpdateReceived,
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
      std::vector<base::OnceClosure> callbacks;
      callbacks.swap(pending_tasks_);
      for (auto& callback : callbacks) {
        std::move(callback).Run();
      }

      // Get update engine status for the initial status. Update engine won't
      // send StatusUpdate signal unless there is a status change. If chrome
      // crashes after UPDATED_NEED_REBOOT status is set, restarted chrome would
      // not get this status. See crbug.com/154104.
      GetUpdateEngineStatus();
    } else {
      LOG(ERROR) << "Failed to wait for D-Bus service to become available";
      pending_tasks_.clear();
    }
  }

  void GetUpdateEngineStatus() {
    // TODO(crbug.com/40633112): Rename the method call back to GetStatus()
    // after the interface changed.
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kGetStatusAdvanced);
    update_engine_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnGetStatus,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&UpdateEngineClientImpl::OnGetStatusError,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void RequestUpdateCheckInternal(UpdateCheckCallback callback,
                                  update_engine::UpdateParams update_params) {
    if (!service_available_) {
      pending_tasks_.push_back(
          base::BindOnce(&UpdateEngineClientImpl::RequestUpdateCheckInternal,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(update_params)));
      return;
    }
    dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                                 update_engine::kUpdate);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(update_params)) {
      LOG(ERROR) << "Failed to encode UpdateParams protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), UPDATE_RESULT_FAILED));
      return;
    }

    VLOG(1) << "Requesting an update";
    // Bind the same callback method for reuse.
    update_engine_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpdateEngineClientImpl::OnRequestUpdateCheck,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Called when a response for RequestUpdateCheck() is received.
  void OnRequestUpdateCheck(UpdateCheckCallback callback,
                            dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request update";
      std::move(callback).Run(UPDATE_RESULT_FAILED);
      return;
    }
    std::move(callback).Run(UPDATE_RESULT_SUCCESS);
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
  void OnCanRollbackCheck(RollbackCheckCallback callback,
                          dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request rollback availability status";
      std::move(callback).Run(false);
      return;
    }
    dbus::MessageReader reader(response);
    bool can_rollback;
    if (!reader.PopBool(&can_rollback)) {
      LOG(ERROR) << "Incorrect response: " << response->ToString();
      std::move(callback).Run(false);
      return;
    }
    VLOG(1) << "Rollback availability status received: " << can_rollback;
    std::move(callback).Run(can_rollback);
  }

  // Called when a response for GetStatus is received.
  void OnGetStatus(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to get response for GetStatus request.";
      return;
    }

    dbus::MessageReader reader(response);
    update_engine::StatusResult status;
    if (!reader.PopArrayOfBytesAsProto(&status)) {
      LOG(ERROR) << "Failed to parse proto from DBus Response.";
      return;
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
  void OnGetChannel(GetChannelCallback callback, dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request getting channel";
      std::move(callback).Run("");
      return;
    }
    dbus::MessageReader reader(response);
    std::string channel;
    if (!reader.PopString(&channel)) {
      LOG(ERROR) << "Incorrect response: " << response->ToString();
      std::move(callback).Run("");
      return;
    }
    VLOG(1) << "The channel received: " << channel;
    std::move(callback).Run(channel);
  }

  // Called when a response for GetStatusAdvanced() is
  // received.
  void OnGetEolInfo(GetEolInfoCallback callback, dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to request getting eol info.";
      std::move(callback).Run(EolInfo());
      return;
    }

    dbus::MessageReader reader(response);
    update_engine::StatusResult status;
    if (!reader.PopArrayOfBytesAsProto(&status)) {
      LOG(ERROR) << "Failed to parse proto from DBus Response.";
      std::move(callback).Run(EolInfo());
      return;
    }

    VLOG(1) << "Eol date received: " << status.eol_date();
    VLOG(1) << "Extended date received: " << status.extended_date();
    VLOG(1) << "Extended opt in received: "
            << status.extended_opt_in_required();

    EolInfo eol_info;
    if (status.eol_date() > 0) {
      eol_info.eol_date =
          base::Time::UnixEpoch() + base::Days(status.eol_date());
    }
    if (status.extended_date() > 0) {
      eol_info.extended_date =
          base::Time::UnixEpoch() + base::Days(status.extended_date());
    }
    eol_info.extended_opt_in_required = status.extended_opt_in_required();

    std::move(callback).Run(eol_info);
  }

  // Called when a response for SetUpdateOverCellularPermission() is received.
  void OnSetUpdateOverCellularPermission(base::OnceClosure callback,
                                         dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << update_engine::kSetUpdateOverCellularPermission
                 << " call failed";
    }

    // Callback should run anyway, regardless of whether DBus call to enable
    // update over cellular succeeded or failed.
    std::move(callback).Run();
  }

  // Called when a response for ToggleFeature() is received.
  void OnToggleFeature(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "ToggleFeature call failed.";
      return;
    }
    VLOG(1) << "Successfully updated feature value.";
  }

  void OnIsFeatureEnabled(const std::string& feature,
                          IsFeatureEnabledCallback callback,
                          dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << update_engine::kIsFeatureEnabled
                 << " call failed for feature " << feature;
      std::move(callback).Run(std::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    bool enabled;
    if (!reader.PopBool(&enabled)) {
      LOG(ERROR) << "Bad response: " << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    VLOG(1) << "Feature " << feature << " is " << enabled;
    std::move(callback).Run(enabled);
  }

  // Called when a response for SetUpdateOverCellularOneTimePermission() is
  // received.
  void OnSetUpdateOverCellularOneTimePermission(
      UpdateOverCellularOneTimePermissionCallback callback,
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

    std::move(callback).Run(success);
  }

  // Called when a response for `ApplyDeferredUpdate()` is received.
  void OnApplyDeferredUpdate(base::OnceClosure failure_callback,
                             dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << update_engine::kApplyDeferredUpdate << " call failed.";
      std::move(failure_callback).Run();
      return;
    }

    VLOG(1) << "Update is applied.";
  }

  // Called when a status update signal is received.
  void StatusUpdateReceived(dbus::Signal* signal) {
    VLOG(1) << "Status update signal received: " << signal->ToString();
    dbus::MessageReader reader(signal);
    update_engine::StatusResult status;
    if (!reader.PopArrayOfBytesAsProto(&status)) {
      LOG(ERROR) << "Failed to parse proto from DBus Response.";
      return;
    }

    last_status_ = status;
    for (auto& observer : observers_)
      observer.UpdateStatusChanged(status);
  }

  // Called when the status update signal is initially connected.
  void StatusUpdateConnected(const std::string& interface_name,
                             const std::string& signal_name,
                             bool success) {
    LOG_IF(WARNING, !success) << "Failed to connect to status updated signal.";
  }

  raw_ptr<dbus::ObjectProxy> update_engine_proxy_;
  base::ObserverList<Observer>::Unchecked observers_;
  update_engine::StatusResult last_status_;

  // True after update_engine's D-Bus service has become available.
  bool service_available_ = false;

  // This is a list of postponed calls to update engine to be called
  // after it becomes available.
  std::vector<base::OnceClosure> pending_tasks_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UpdateEngineClientImpl> weak_ptr_factory_{this};
};

// The UpdateEngineClient implementation used on Linux desktop,
// which does nothing.
class UpdateEngineClientDesktopFake : public UpdateEngineClient {
 public:
  UpdateEngineClientDesktopFake()
      : current_channel_("beta-channel"), target_channel_("beta-channel") {}

  UpdateEngineClientDesktopFake(const UpdateEngineClientDesktopFake&) = delete;
  UpdateEngineClientDesktopFake& operator=(
      const UpdateEngineClientDesktopFake&) = delete;

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

  void RequestUpdateCheckInternal(UpdateCheckCallback callback,
                                  bool apply_update) {
    if (last_status_.current_operation() != update_engine::Operation::IDLE) {
      std::move(callback).Run(UPDATE_RESULT_FAILED);
      return;
    }
    std::move(callback).Run(UPDATE_RESULT_SUCCESS);
    last_status_.set_current_operation(
        update_engine::Operation::CHECKING_FOR_UPDATE);
    last_status_.set_progress(0.0);
    last_status_.set_last_checked_time(0);
    last_status_.set_new_version("0.0.0.0");
    last_status_.set_new_size(0);
    last_status_.set_is_enterprise_rollback(false);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UpdateEngineClientDesktopFake::StateTransition,
                       weak_factory_.GetWeakPtr(), apply_update),
        base::Milliseconds(kStateTransitionDefaultDelayMs));
  }

  void RequestUpdateCheck(UpdateCheckCallback callback) override {
    RequestUpdateCheckInternal(std::move(callback), true);
  }

  void RequestUpdateCheckWithoutApplying(
      UpdateCheckCallback callback) override {
    RequestUpdateCheckInternal(std::move(callback), false);
  }

  void RebootAfterUpdate() override {}

  void Rollback() override {}

  void CanRollbackCheck(RollbackCheckCallback callback) override {
    std::move(callback).Run(true);
  }

  update_engine::StatusResult GetLastStatus() override { return last_status_; }

  void SetChannel(const std::string& target_channel,
                  bool is_powerwash_allowed) override {
    VLOG(1) << "Requesting to set channel: "
            << "target_channel=" << target_channel << ", "
            << "is_powerwash_allowed=" << is_powerwash_allowed;
    target_channel_ = target_channel;
  }
  void GetChannel(bool get_current_channel,
                  GetChannelCallback callback) override {
    VLOG(1) << "Requesting to get channel, get_current_channel="
            << get_current_channel;
    if (get_current_channel)
      std::move(callback).Run(current_channel_);
    else
      std::move(callback).Run(target_channel_);
  }

  void GetEolInfo(GetEolInfoCallback callback) override {
    std::move(callback).Run(EolInfo());
  }

  void SetUpdateOverCellularPermission(bool allowed,
                                       base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  void SetUpdateOverCellularOneTimePermission(
      const std::string& update_version,
      int64_t update_size,
      UpdateOverCellularOneTimePermissionCallback callback) override {}

  void ToggleFeature(const std::string& feature, bool enable) override {
    VLOG(1) << "Requesting to set " << feature
            << " to: " << (enable ? "enabled" : "disabled");
  }

  void IsFeatureEnabled(const std::string& feature,
                        IsFeatureEnabledCallback callback) override {
    VLOG(1) << "Requesting to get " << feature;
    std::move(callback).Run(std::nullopt);
  }

  void ApplyDeferredUpdate(bool shutdown_after_update,
                           base::OnceClosure failure_callback) override {
    VLOG(1) << "Applying deferred update and "
            << (shutdown_after_update ? "shutdown." : "reboot.");
  }

 private:
  void StateTransition(bool apply_update) {
    update_engine::Operation next_operation = update_engine::Operation::ERROR;
    int delay_ms = kStateTransitionDefaultDelayMs;
    switch (last_status_.current_operation()) {
      case update_engine::Operation::ERROR:
      case update_engine::Operation::IDLE:
      case update_engine::Operation::UPDATED_NEED_REBOOT:
      case update_engine::Operation::REPORTING_ERROR_EVENT:
      case update_engine::Operation::ATTEMPTING_ROLLBACK:
      case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
      case update_engine::Operation::DISABLED:
      case update_engine::Operation::UPDATED_BUT_DEFERRED:
      case update_engine::Operation::CLEANUP_PREVIOUS_UPDATE:
        return;
      case update_engine::Operation::CHECKING_FOR_UPDATE:
        next_operation = update_engine::Operation::UPDATE_AVAILABLE;
        break;
      case update_engine::Operation::UPDATE_AVAILABLE:
        next_operation = apply_update ? update_engine::Operation::DOWNLOADING
                                      : update_engine::Operation::IDLE;
        break;
      case update_engine::Operation::DOWNLOADING:
        if (last_status_.progress() >= 1.0) {
          next_operation = update_engine::Operation::VERIFYING;
        } else {
          next_operation = update_engine::Operation::DOWNLOADING;
          last_status_.set_progress(last_status_.progress() + 0.01);
          last_status_.set_new_version(kStubVersion);
          last_status_.set_new_size(kDownloadSizeDelta);
          delay_ms = kStateTransitionDownloadingDelayMs;
        }
        break;
      case update_engine::Operation::VERIFYING:
        next_operation = update_engine::Operation::FINALIZING;
        break;
      case update_engine::Operation::FINALIZING:
        next_operation = update_engine::Operation::UPDATED_NEED_REBOOT;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    last_status_.set_current_operation(next_operation);
    for (auto& observer : observers_)
      observer.UpdateStatusChanged(last_status_);
    if (last_status_.current_operation() != update_engine::Operation::IDLE) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&UpdateEngineClientDesktopFake::StateTransition,
                         weak_factory_.GetWeakPtr(), apply_update),
          base::Milliseconds(delay_ms));
    }
  }

  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observers_;

  std::string current_channel_;
  std::string target_channel_;

  update_engine::StatusResult last_status_;

  base::WeakPtrFactory<UpdateEngineClientDesktopFake> weak_factory_{this};
};

}  // namespace

// static
UpdateEngineClient* UpdateEngineClient::Get() {
  return g_instance;
}

// static
void UpdateEngineClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new UpdateEngineClientImpl())->Init(bus);
}

// static
void UpdateEngineClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test to
  // allow the test to set its own client.
  if (g_instance)
    return;

  (new UpdateEngineClientDesktopFake())->Init(nullptr);
}

// static
FakeUpdateEngineClient* UpdateEngineClient::InitializeFakeForTest() {
  auto* client = new FakeUpdateEngineClient();
  client->Init(nullptr);
  return client;
}

// static
void UpdateEngineClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

UpdateEngineClient::UpdateEngineClient() {
  CHECK(!g_instance);
  g_instance = this;
}

UpdateEngineClient::~UpdateEngineClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
