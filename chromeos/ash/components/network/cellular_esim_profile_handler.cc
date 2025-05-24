// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/cellular_esim_profile_waiter.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/hermes_metrics_util.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_event_log.h"

namespace ash {
namespace {

// The timeout that is provided when waiting for the properties of discovered
// pending profiles to be set before collecting profile properties for
// RequestAvailableProfiles().
constexpr base::TimeDelta kCellularESimProfileWaiterDelay = base::Seconds(30);

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

CellularESimProfileHandler::RequestAvailableProfilesInfo::
    RequestAvailableProfilesInfo() = default;

CellularESimProfileHandler::RequestAvailableProfilesInfo::
    ~RequestAvailableProfilesInfo() = default;

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

void CellularESimProfileHandler::RequestAvailableProfiles(
    const dbus::ObjectPath& euicc_path,
    RequestAvailableProfilesCallback callback) {
  std::unique_ptr<RequestAvailableProfilesInfo> info =
      std::make_unique<RequestAvailableProfilesInfo>();
  info->smds_activation_codes = cellular_utils::GetSmdsActivationCodes();
  info->callback = std::move(callback);

  cellular_inhibitor_->InhibitCellularScanning(
      CellularInhibitor::InhibitReason::kRequestingAvailableProfiles,
      base::BindOnce(
          &CellularESimProfileHandler::OnInhibitedForRequestAvailableProfiles,
          weak_ptr_factory_.GetWeakPtr(), euicc_path, std::move(info)));
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
      base::BindOnce(
          &CellularESimProfileHandler::OnInhibitedForRefreshProfileList,
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

void CellularESimProfileHandler::OnInhibitedForRefreshProfileList(
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
    hermes_metrics::LogRefreshInstalledProfilesLatency(call_latency);
    has_completed_successful_profile_refresh_ = true;
    OnHermesPropertiesUpdated();
  }

  // TODO(crbug.com/1216693) Update with more robust way of waiting for eSIM
  // profile objects to be loaded.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(inhibit_lock_)),
      kProfileRefreshCallbackDelay);
}

void CellularESimProfileHandler::OnInhibitedForRequestAvailableProfiles(
    const dbus::ObjectPath& euicc_path,
    std::unique_ptr<RequestAvailableProfilesInfo> info,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK(info);
  DCHECK(info->callback);

  if (!inhibit_lock) {
    DCHECK(!info->smds_activation_codes.empty());
    NET_LOG(ERROR)
        << "Failed to inhibit cellular for requesting available profiles";
    CellularNetworkMetricsLogger::LogSmdsScanResult(
        info->smds_activation_codes.front(),
        /*result=*/std::nullopt);
    std::move(info->callback)
        .Run(cellular_setup::mojom::ESimOperationResult::kFailure,
             std::vector<CellularESimProfile>());
    return;
  }

  PerformRequestAvailableProfiles(euicc_path, std::move(info),
                                  std::move(inhibit_lock));
}

void CellularESimProfileHandler::PerformRequestAvailableProfiles(
    const dbus::ObjectPath& euicc_path,
    std::unique_ptr<RequestAvailableProfilesInfo> info,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock) {
  DCHECK(info);
  DCHECK(info->callback);
  DCHECK(inhibit_lock);

  if (!info->smds_activation_codes.empty()) {
    NET_LOG(EVENT) << "Requesting available profiles";

    // Remove one SM-DS activation code from the list and use this activation
    // code for the next SM-DS scan. This logic is responsible for making sure
    // we only scan each activation code once, avoiding an infinite loop.
    const std::string smds_activation_code = info->smds_activation_codes.back();
    info->smds_activation_codes.pop_back();

    HermesEuiccClient::Get()->RefreshSmdxProfiles(
        euicc_path, smds_activation_code,
        /*restore_slot=*/true,
        base::BindOnce(&CellularESimProfileHandler::OnRequestAvailableProfiles,
                       weak_ptr_factory_.GetWeakPtr(), euicc_path,
                       std::move(info), std::move(inhibit_lock),
                       smds_activation_code, base::TimeTicks::Now()));
    return;
  }

  NET_LOG(EVENT) << "Finished requesting available profiles";

  CellularNetworkMetricsLogger::LogSmdsScanProfileCount(
      info->profile_paths.size(),
      CellularNetworkMetricsLogger::SmdsScanMethod::kViaUser);

  std::unique_ptr<CellularESimProfileWaiter> waiter =
      std::make_unique<CellularESimProfileWaiter>();
  for (const auto& profile_path : info->profile_paths) {
    waiter->RequirePendingProfile(profile_path);
  }

  auto split = base::SplitOnceCallback(std::move(info->callback));
  info->callback = std::move(split.first);

  auto on_success = base::BindOnce(base::BindOnce(
      &CellularESimProfileHandler::CompleteRequestAvailableProfiles,
      weak_ptr_factory_.GetWeakPtr(), euicc_path, std::move(info)));
  auto on_shutdown = base::BindOnce(
      [](RequestAvailableProfilesCallback callback) {
        std::move(callback).Run(
            cellular_setup::mojom::ESimOperationResult::kSuccess,
            std::vector<CellularESimProfile>());
      },
      std::move(split.second));

  waiter->Wait(std::move(on_success), std::move(on_shutdown));

  if (waiter->waiting()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<CellularESimProfileWaiter> waiter) {
              waiter.reset();
            },
            std::move(waiter)),
        kCellularESimProfileWaiterDelay);
  }
}

void CellularESimProfileHandler::OnRequestAvailableProfiles(
    const dbus::ObjectPath& euicc_path,
    std::unique_ptr<RequestAvailableProfilesInfo> info,
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
    const std::string& smds_activation_code,
    const base::TimeTicks start_time,
    HermesResponseStatus status,
    const std::vector<dbus::ObjectPath>& profile_paths) {
  DCHECK(info);
  DCHECK(info->callback);
  DCHECK(inhibit_lock);

  CellularNetworkMetricsLogger::LogSmdsScanResult(smds_activation_code, status);
  CellularNetworkMetricsLogger::LogSmdsScanDuration(
      base::TimeTicks::Now() - start_time,
      status == HermesResponseStatus::kSuccess, smds_activation_code);

  for (const auto& profile_path : profile_paths) {
    info->profile_paths.emplace_back(profile_path.value());
  }

  // This function is provided as a callback to
  // PerformRequestAvailableProfiles() to be called when an SM-DS scan
  // completes. Since the activation code used in this function may not have
  // been the last needed, continue the loop. When |info.smds_activation_codes|
  // is empty PerformRequestAvailableProfiles() will exit this loop by invoking
  // |info.callback|.
  PerformRequestAvailableProfiles(euicc_path, std::move(info),
                                  std::move(inhibit_lock));
}

void CellularESimProfileHandler::CompleteRequestAvailableProfiles(
    const dbus::ObjectPath& euicc_path,
    std::unique_ptr<RequestAvailableProfilesInfo> info) {
  DCHECK(info);
  DCHECK(info->callback);

  std::vector<CellularESimProfile> profile_list;

  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(euicc_path);
  DCHECK(euicc_properties);

  for (const auto& profile_path : info->profile_paths) {
    HermesProfileClient::Properties* profile_properties =
        HermesProfileClient::Get()->GetProperties(profile_path);
    if (!profile_properties) {
      NET_LOG(ERROR)
          << "Failed to get profile properties for available profile";
      continue;
    }
    if (profile_properties->state().value() !=
        hermes::profile::State::kPending) {
      NET_LOG(ERROR) << "Expected available profile to have state "
                     << hermes::profile::State::kPending << ", has "
                     << profile_properties->state().value();
      continue;
    }
    if (profile_properties->name().value().empty()) {
      NET_LOG(ERROR) << "Expected available profile to have a non-empty name";
      continue;
    }
    if (profile_properties->activation_code().value().empty()) {
      NET_LOG(ERROR)
          << "Expected available profile to have a non-empty activation code";
      continue;
    }

    NET_LOG(EVENT) << "Found available profile";

    profile_list.emplace_back(
        CellularESimProfile::State::kPending, profile_path,
        euicc_properties->eid().value(), profile_properties->iccid().value(),
        base::UTF8ToUTF16(profile_properties->name().value()),
        base::UTF8ToUTF16(profile_properties->nick_name().value()),
        base::UTF8ToUTF16(profile_properties->service_provider().value()),
        profile_properties->activation_code().value());
  }

  std::move(info->callback)
      .Run(cellular_setup::mojom::ESimOperationResult::kSuccess,
           std::move(profile_list));
}

}  // namespace ash
