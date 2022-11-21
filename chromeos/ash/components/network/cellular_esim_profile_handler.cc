// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"

#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/network/cellular_esim_profile.h"
#include "chromeos/ash/components/network/hermes_metrics_util.h"

namespace ash {
namespace {

// Delay before profile refresh callback is called. This ensures that eSIM
// profiles are updated before callback returns.
constexpr base::TimeDelta kProfileRefreshCallbackDelay =
    base::Milliseconds(150);

}  // namespace

CellularESimProfileHandler::CellularESimProfileHandler() = default;

CellularESimProfileHandler::~CellularESimProfileHandler() {
  HermesManagerClient::Get()->RemoveObserver(this);
  HermesEuiccClient::Get()->RemoveObserver(this);
  HermesProfileClient::Get()->RemoveObserver(this);
}

void CellularESimProfileHandler::Init(
    NetworkStateHandler* network_state_handler,
    CellularInhibitor* cellular_inhibitor) {
  network_state_handler_ = network_state_handler;
  cellular_inhibitor_ = cellular_inhibitor;
  HermesManagerClient::Get()->AddObserver(this);
  HermesEuiccClient::Get()->AddObserver(this);
  HermesProfileClient::Get()->AddObserver(this);
  InitInternal();
}

void CellularESimProfileHandler::RefreshProfileList(
    const dbus::ObjectPath& euicc_path,
    RefreshProfilesCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  PerformRefreshProfileList(euicc_path, /*restore_slot=*/false,
                            std::move(callback), std::move(inhibit_lock));
}

void CellularESimProfileHandler::RefreshProfileListAndRestoreSlot(
    const dbus::ObjectPath& euicc_path,
    RefreshProfilesCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  PerformRefreshProfileList(euicc_path, /*restore_slot=*/true,
                            std::move(callback), std::move(inhibit_lock));
}

void CellularESimProfileHandler::PerformRefreshProfileList(
    const dbus::ObjectPath& euicc_path,
    bool restore_slot,
    RefreshProfilesCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (inhibit_lock) {
    RefreshProfilesWithLock(euicc_path, restore_slot, std::move(callback),
                            std::move(inhibit_lock));
    return;
  }

  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kRefreshingProfileList,
      base::BindOnce(&CellularESimProfileHandler::OnInhibited,
                     weak_ptr_factory_.GetWeakPtr(), euicc_path, restore_slot,
                     std::move(callback)));
}

void CellularESimProfileHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CellularESimProfileHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void CellularESimProfileHandler::NotifyESimProfileListUpdated() {
  for (auto& observer : observer_list_)
    observer.OnESimProfileListUpdated();
}

void CellularESimProfileHandler::OnAvailableEuiccListChanged() {
  OnHermesPropertiesUpdated();
}

void CellularESimProfileHandler::OnEuiccPropertyChanged(
    const dbus::ObjectPath& euicc_path,
    const std::string& property_name) {
  OnHermesPropertiesUpdated();
}

void CellularESimProfileHandler::OnCarrierProfilePropertyChanged(
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& property_name) {
  OnHermesPropertiesUpdated();
}

void CellularESimProfileHandler::OnInhibited(
    const dbus::ObjectPath& euicc_path,
    bool restore_slot,
    RefreshProfilesCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  if (!inhibit_lock) {
    std::move(callback).Run(nullptr);
    return;
  }

  RefreshProfilesWithLock(euicc_path, restore_slot, std::move(callback),
                          std::move(inhibit_lock));
}

void CellularESimProfileHandler::RefreshProfilesWithLock(
    const dbus::ObjectPath& euicc_path,
    bool restore_slot,
    RefreshProfilesCallback callback,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK(inhibit_lock);

  // Only one profile refresh should be in progress at a time. Since we are
  // about to start a new refresh, we expect that |callback_| and
  // |inhibit_lock_| are null.
  DCHECK(!callback_);
  DCHECK(!inhibit_lock_);

  // Set instance fields which track ongoing refresh attempts.
  inhibit_lock_ = std::move(inhibit_lock);
  callback_ = std::move(callback);

  base::TimeTicks start_time = base::TimeTicks::Now();
  HermesEuiccClient::Get()->RefreshInstalledProfiles(
      euicc_path, restore_slot,
      base::BindOnce(
          &CellularESimProfileHandler::OnRequestInstalledProfilesResult,
          weak_ptr_factory_.GetWeakPtr(), start_time));
}

void CellularESimProfileHandler::OnRequestInstalledProfilesResult(
    base::TimeTicks start_time,
    HermesResponseStatus status) {
  DCHECK(inhibit_lock_);
  DCHECK(callback_);

  base::TimeDelta call_latency = base::TimeTicks::Now() - start_time;

  // If the operation failed, reset |inhibit_lock_| before it is returned to
  // the callback below to indicate failure.
  if (status != HermesResponseStatus::kSuccess) {
    inhibit_lock_.reset();
  } else {
    hermes_metrics::LogRequestPendingProfilesLatency(call_latency);
    has_completed_successful_profile_refresh_ = true;
    OnHermesPropertiesUpdated();
  }

  // TODO(crbug.com/1216693) Update with more robust way of waiting for eSIM
  // profile objects to be loaded.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(inhibit_lock_)),
      kProfileRefreshCallbackDelay);
}

}  // namespace ash
