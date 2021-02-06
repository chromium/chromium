// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_inhibitor.h"
#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

// Delay for first uninhibit retry attempt. Delay doubles for every
// subsequent attempt.
const base::TimeDelta kUninhibitRetryDelay = base::TimeDelta::FromSeconds(2);

}  // namespace

CellularInhibitor::InhibitLock::InhibitLock(base::OnceClosure unlock_callback)
    : unlock_callback_(std::move(unlock_callback)) {}

CellularInhibitor::InhibitLock::~InhibitLock() {
  std::move(unlock_callback_).Run();
}

CellularInhibitor::CellularInhibitor() = default;

CellularInhibitor::~CellularInhibitor() = default;

void CellularInhibitor::Init(NetworkStateHandler* network_state_handler,
                             NetworkDeviceHandler* network_device_handler) {
  network_state_handler_ = network_state_handler;
  network_device_handler_ = network_device_handler;
}

void CellularInhibitor::InhibitCellularScanning(InhibitCallback callback) {
  inhibit_requests_.push(std::move(callback));
  ProcessRequests();
}

const DeviceState* CellularInhibitor::GetCellularDevice() const {
  return network_state_handler_->GetDeviceStateByType(
      NetworkTypePattern::Cellular());
}

void CellularInhibitor::ProcessRequests() {
  if (inhibit_requests_.empty()) {
    return;
  }

  if (is_locked_) {
    // Inhibitor is already locked. Wait till next ProcessRequest call.
    return;
  }

  is_locked_ = true;
  SetInhibitProperty(/*new_inhibit_value=*/true,
                     base::BindOnce(&CellularInhibitor::OnInhibit,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void CellularInhibitor::OnInhibit(bool success) {
  if (success) {
    std::unique_ptr<InhibitLock> lock = std::make_unique<InhibitLock>(
        base::BindOnce(&CellularInhibitor::AttemptUninhibit,
                       weak_ptr_factory_.GetWeakPtr(), /*attempts_so_far=*/0));
    std::move(inhibit_requests_.front()).Run(std::move(lock));
  } else {
    std::move(inhibit_requests_.front()).Run(nullptr);
    PopRequestAndProcessNext();
  }
}

void CellularInhibitor::AttemptUninhibit(size_t attempts_so_far) {
  SetInhibitProperty(
      /*new_inhibit_value=*/false,
      base::BindOnce(&CellularInhibitor::OnUninhibit,
                     weak_ptr_factory_.GetWeakPtr(), attempts_so_far));
}

void CellularInhibitor::OnUninhibit(size_t attempts_so_far, bool success) {
  if (!success) {
    base::TimeDelta retry_delay = kUninhibitRetryDelay * (1 << attempts_so_far);
    NET_LOG(DEBUG) << "Uninhibit Failed. Retrying in " << retry_delay;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CellularInhibitor::AttemptUninhibit,
                       weak_ptr_factory_.GetWeakPtr(), attempts_so_far + 1),
        retry_delay);
    return;
  }
  PopRequestAndProcessNext();
}

void CellularInhibitor::PopRequestAndProcessNext() {
  inhibit_requests_.pop();
  is_locked_ = false;
  ProcessRequests();
}

void CellularInhibitor::SetInhibitProperty(bool new_inhibit_value,
                                           SuccessCallback callback) {
  const DeviceState* cellular_device = GetCellularDevice();
  if (!cellular_device) {
    std::move(callback).Run(false);
    return;
  }

  // If the new value is already set, return early.
  if (cellular_device->inhibited() == new_inhibit_value) {
    std::move(callback).Run(true);
    return;
  }

  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  network_device_handler_->SetDeviceProperty(
      cellular_device->path(), shill::kInhibitedProperty,
      base::Value(new_inhibit_value),
      base::BindOnce(&CellularInhibitor::OnSetPropertySuccess,
                     weak_ptr_factory_.GetWeakPtr(), repeating_callback),
      base::BindOnce(&CellularInhibitor::OnSetPropertyError,
                     weak_ptr_factory_.GetWeakPtr(), repeating_callback,
                     /*attempted_inhibit=*/new_inhibit_value));
}

void CellularInhibitor::OnSetPropertySuccess(
    const base::RepeatingCallback<void(bool)>& success_callback) {
  std::move(success_callback).Run(true);
}

void CellularInhibitor::OnSetPropertyError(
    const base::RepeatingCallback<void(bool)>& success_callback,
    bool attempted_inhibit,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG(ERROR) << (attempted_inhibit ? "Inhibit" : "Uninhibit")
                 << "CellularScanning() failed: " << error_name;
  std::move(success_callback).Run(false);
}

}  // namespace chromeos
