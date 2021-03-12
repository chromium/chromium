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
constexpr base::TimeDelta kUninhibitRetryDelay =
    base::TimeDelta::FromSeconds(2);

}  // namespace

CellularInhibitor::InhibitLock::InhibitLock(base::OnceClosure unlock_callback)
    : unlock_callback_(std::move(unlock_callback)) {}

CellularInhibitor::InhibitLock::~InhibitLock() {
  std::move(unlock_callback_).Run();
}

CellularInhibitor::CellularInhibitor() = default;

CellularInhibitor::~CellularInhibitor() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void CellularInhibitor::Init(NetworkStateHandler* network_state_handler,
                             NetworkDeviceHandler* network_device_handler) {
  network_state_handler_ = network_state_handler;
  network_device_handler_ = network_device_handler;

  network_state_handler_->AddObserver(this, FROM_HERE);
}

void CellularInhibitor::InhibitCellularScanning(InhibitCallback callback) {
  inhibit_requests_.push(std::move(callback));
  ProcessRequests();
}

void CellularInhibitor::DeviceListChanged() {
  CheckScanningIfNeeded();
}

void CellularInhibitor::DevicePropertiesUpdated(const DeviceState* device) {
  CheckScanningIfNeeded();
}

const DeviceState* CellularInhibitor::GetCellularDevice() const {
  return network_state_handler_->GetDeviceStateByType(
      NetworkTypePattern::Cellular());
}

void CellularInhibitor::TransitionToState(State state) {
  NET_LOG(EVENT) << "CellularInhibitor state: " << state_ << " => " << state;
  state_ = state;
}

void CellularInhibitor::ProcessRequests() {
  if (inhibit_requests_.empty())
    return;

  // Another inhibit request is already underway; wait until it has completed
  // before starting a new request.
  if (state_ != State::kIdle)
    return;

  TransitionToState(State::kInhibiting);
  SetInhibitProperty(/*new_inhibit_value=*/true,
                     base::BindOnce(&CellularInhibitor::OnInhibit,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void CellularInhibitor::OnInhibit(bool success) {
  DCHECK(state_ == State::kInhibiting);

  if (success) {
    TransitionToState(State::kInhibited);
    std::unique_ptr<InhibitLock> lock = std::make_unique<InhibitLock>(
        base::BindOnce(&CellularInhibitor::AttemptUninhibit,
                       weak_ptr_factory_.GetWeakPtr(), /*attempts_so_far=*/0));
    std::move(inhibit_requests_.front()).Run(std::move(lock));
    return;
  }

  std::move(inhibit_requests_.front()).Run(nullptr);
  PopRequestAndProcessNext();
}

void CellularInhibitor::AttemptUninhibit(size_t attempts_so_far) {
  DCHECK(state_ == State::kInhibited);
  TransitionToState(State::kUninhibiting);
  SetInhibitProperty(
      /*new_inhibit_value=*/false,
      base::BindOnce(&CellularInhibitor::OnUninhibit,
                     weak_ptr_factory_.GetWeakPtr(), attempts_so_far));
}

void CellularInhibitor::OnUninhibit(size_t attempts_so_far, bool success) {
  DCHECK(state_ == State::kUninhibiting);

  if (!success) {
    base::TimeDelta retry_delay = kUninhibitRetryDelay * (1 << attempts_so_far);
    NET_LOG(DEBUG) << "Uninhibit Failed. Retrying in " << retry_delay;
    TransitionToState(State::kInhibited);

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CellularInhibitor::AttemptUninhibit,
                       weak_ptr_factory_.GetWeakPtr(), attempts_so_far + 1),
        retry_delay);
    return;
  }

  TransitionToState(State::kWaitingForScanningToStart);
  CheckForScanningStarted();
}

void CellularInhibitor::CheckScanningIfNeeded() {
  if (state_ == State::kWaitingForScanningToStart)
    CheckForScanningStarted();

  if (state_ == State::kWaitingForScanningToStop)
    CheckForScanningStopped();
}

void CellularInhibitor::CheckForScanningStarted() {
  DCHECK(state_ == State::kWaitingForScanningToStart);

  if (!HasScanningStarted())
    return;

  TransitionToState(State::kWaitingForScanningToStop);
  CheckForScanningStopped();
}

bool CellularInhibitor::HasScanningStarted() {
  const DeviceState* cellular_device = GetCellularDevice();
  if (!cellular_device)
    return false;
  return !cellular_device->inhibited() && cellular_device->scanning();
}

void CellularInhibitor::CheckForScanningStopped() {
  DCHECK(state_ == State::kWaitingForScanningToStop);

  if (!HasScanningStopped())
    return;

  PopRequestAndProcessNext();
}

bool CellularInhibitor::HasScanningStopped() {
  const DeviceState* cellular_device = GetCellularDevice();
  if (!cellular_device)
    return false;
  return !cellular_device->scanning();
}

void CellularInhibitor::PopRequestAndProcessNext() {
  inhibit_requests_.pop();
  TransitionToState(State::kIdle);
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

std::ostream& operator<<(std::ostream& stream,
                         const CellularInhibitor::State& state) {
  switch (state) {
    case CellularInhibitor::State::kIdle:
      stream << "[Idle]";
      break;
    case CellularInhibitor::State::kInhibiting:
      stream << "[Inhibiting]";
      break;
    case CellularInhibitor::State::kInhibited:
      stream << "[Inhibited]";
      break;
    case CellularInhibitor::State::kUninhibiting:
      stream << "[Uninhibiting]";
      break;
    case CellularInhibitor::State::kWaitingForScanningToStart:
      stream << "[Waiting for scanning to start]";
      break;
    case CellularInhibitor::State::kWaitingForScanningToStop:
      stream << "[Waiting for scanning to stop]";
      break;
  }
  return stream;
}

}  // namespace chromeos
