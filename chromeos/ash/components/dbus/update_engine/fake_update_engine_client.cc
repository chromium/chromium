// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeUpdateEngineClient::FakeUpdateEngineClient() {}

FakeUpdateEngineClient::~FakeUpdateEngineClient() = default;

void FakeUpdateEngineClient::Init(dbus::Bus* bus) {}

void FakeUpdateEngineClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeUpdateEngineClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeUpdateEngineClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void FakeUpdateEngineClient::RequestUpdateCheck(UpdateCheckCallback callback) {
  request_update_check_call_count_++;
  std::move(callback).Run(update_check_result_);
}

void FakeUpdateEngineClient::RequestUpdateCheckWithoutApplying(
    UpdateCheckCallback callback) {
  request_update_check_without_applying_call_count_++;
  std::move(callback).Run(update_check_result_);
}

void FakeUpdateEngineClient::Rollback() {
  rollback_call_count_++;
}

void FakeUpdateEngineClient::CanRollbackCheck(RollbackCheckCallback callback) {
  can_rollback_call_count_++;
  std::move(callback).Run(can_rollback_stub_result_);
}

void FakeUpdateEngineClient::RebootAfterUpdate() {
  if (reboot_after_update_callback_) {
    std::move(reboot_after_update_callback_).Run();
  }
  reboot_after_update_call_count_++;
}

update_engine::StatusResult FakeUpdateEngineClient::GetLastStatus() {
  if (status_queue_.empty())
    return default_status_;

  update_engine::StatusResult last_status = status_queue_.front();
  status_queue_.pop();
  return last_status;
}

void FakeUpdateEngineClient::NotifyObserversThatStatusChanged(
    const update_engine::StatusResult& status) {
  for (auto& observer : observers_)
    observer.UpdateStatusChanged(status);
}

void FakeUpdateEngineClient::
    NotifyUpdateOverCellularOneTimePermissionGranted() {
  for (auto& observer : observers_)
    observer.OnUpdateOverCellularOneTimePermissionGranted();
}

void FakeUpdateEngineClient::SetChannel(const std::string& target_channel,
                                        bool is_powerwash_allowed) {}

void FakeUpdateEngineClient::GetChannel(bool get_current_channel,
                                        GetChannelCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::string()));
}

void FakeUpdateEngineClient::GetEolInfo(GetEolInfoCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), eol_info_));
}

void FakeUpdateEngineClient::SetUpdateOverCellularPermission(
    bool allowed,
    base::OnceClosure callback) {
  update_over_cellular_permission_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeUpdateEngineClient::SetUpdateOverCellularOneTimePermission(
    const std::string& target_version,
    int64_t target_size,
    UpdateOverCellularOneTimePermissionCallback callback) {
  update_over_cellular_one_time_permission_count_++;
  std::move(callback).Run(true);
}

void FakeUpdateEngineClient::ToggleFeature(const std::string& feature,
                                           bool enable) {
  toggle_feature_count_++;
}

void FakeUpdateEngineClient::IsFeatureEnabled(
    const std::string& feature,
    IsFeatureEnabledCallback callback) {
  is_feature_enabled_count_++;
  std::move(callback).Run(features_.count(feature) ? features_[feature]
                                                   : std::nullopt);
}

void FakeUpdateEngineClient::ApplyDeferredUpdate(
    bool shutdown_after_update,
    base::OnceClosure failure_callback) {
  apply_deferred_update_count_++;
}

void FakeUpdateEngineClient::set_default_status(
    const update_engine::StatusResult& status) {
  default_status_ = status;
}

void FakeUpdateEngineClient::set_update_check_result(
    const UpdateEngineClient::UpdateCheckResult& result) {
  update_check_result_ = result;
}

void FakeUpdateEngineClient::SetToggleFeature(const std::string& feature,
                                              std::optional<bool> opt_enabled) {
  features_[feature] = opt_enabled;
}

}  // namespace ash
