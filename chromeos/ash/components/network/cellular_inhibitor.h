// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_INHIBITOR_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_INHIBITOR_H_

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class DeviceState;
class NetworkStateHandler;
class NetworkDeviceHandler;

// Updates the "Inhibited" property of the Cellular device.
//
// When some SIM-related operations are performed, properties of the Cellular
// device can change to a temporary value and then change back. To prevent churn
// in these properties, Shill provides the "Inhibited" property to inhibit any
// scans.
//
// This class is intended to be used when performing such actions to ensure that
// these transient states never occur.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularInhibitor
    : public NetworkStateHandlerObserver {
 public:
  CellularInhibitor();
  CellularInhibitor(const CellularInhibitor&) = delete;
  CellularInhibitor& operator=(const CellularInhibitor&) = delete;
  ~CellularInhibitor() override;

  void Init(NetworkStateHandler* network_state_handler,
            NetworkDeviceHandler* network_device_handler);

  // A lock object which ensures that all other Inhibit requests are blocked
  // during this it's lifetime. When a lock object is deleted, the Cellular
  // device is automatically uninhibited and any pending inhibit requests are
  // processed.
  class InhibitLock {
   public:
    explicit InhibitLock(base::OnceClosure unlock_callback);
    ~InhibitLock();

   private:
    base::OnceClosure unlock_callback_;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when the inhibit state has changed; observers should use the
    // GetInhibitReason() function to determine the current state.
    virtual void OnInhibitStateChanged() = 0;
  };

  enum class InhibitReason {
    kInstallingProfile,
    kRenamingProfile,
    kRemovingProfile,
    kConnectingToProfile,
    kRefreshingProfileList,
    kResettingEuiccMemory,
    kDisablingProfile,
    kRequestingAvailableProfiles,
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const InhibitReason& state);

  // Callback which returns InhibitLock on inhibit success or nullptr on
  // failure.
  using InhibitCallback =
      base::OnceCallback<void(std::unique_ptr<InhibitLock>)>;

  // This function attempts to put the cellular device into an inhibited state.
  // On success, this method will provide a lock to |callback| that will prevent
  // the cellular device from becoming uninhibited until the lock is freed. On
  // failure, e.g. this function fails to set the corresponding Shill device
  // property, |nullptr| is provided to |callback|.
  void InhibitCellularScanning(InhibitReason reason, InhibitCallback callback);

  // Returns the reason that cellular scanning is currently inhibited, or null
  // if it is not inhibited.
  std::optional<InhibitReason> GetInhibitReason() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 protected:
  void NotifyInhibitStateChanged();

 private:
  FRIEND_TEST_ALL_PREFIXES(CellularInhibitorTest, SuccessSingleRequest);
  FRIEND_TEST_ALL_PREFIXES(CellularInhibitorTest, SuccessMultipleRequests);
  FRIEND_TEST_ALL_PREFIXES(CellularInhibitorTest, Failure);
  FRIEND_TEST_ALL_PREFIXES(CellularInhibitorTest, FailurePropertySetTimeout);
  FRIEND_TEST_ALL_PREFIXES(CellularInhibitorTest, FailureScanningChangeTimeout);
  friend class CellularInhibitorTest;

  // Timeout after which an inhibit property change is considered to be failed.
  static const base::TimeDelta kInhibitPropertyChangeTimeout;

  struct InhibitRequest {
    InhibitRequest(InhibitReason inhibit_reason,
                   InhibitCallback inhibit_callback);
    InhibitRequest(const InhibitRequest&) = delete;
    InhibitRequest& operator=(const InhibitRequest&) = delete;
    ~InhibitRequest();

    InhibitReason inhibit_reason;
    InhibitCallback inhibit_callback;
  };

  enum class State {
    kIdle,
    kInhibiting,
    kWaitForInhibit,
    kInhibited,
    kUninhibiting,
    kWaitForUninhibit,
    kWaitingForScanningToStop,
  };
  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class InhibitOperationResult {
    kSuccess = 0,
    kUninhibitTimeout = 1,
    kSetInhibitFailed = 2,
    kSetInhibitTimeout = 3,
    kSetInhibitNoDevice = 4,
    kMaxValue = kSetInhibitNoDevice
  };
  static void RecordInhibitOperationResult(InhibitOperationResult result);

  // NetworkStateHandlerObserver:
  void DeviceListChanged() override;
  void DevicePropertiesUpdated(const DeviceState* device) override;

  const DeviceState* GetCellularDevice() const;

  void TransitionToState(State state);
  void ProcessRequests();
  // Called when inhibit completes. |result| is the operation error result and
  // is set only for failures.
  void OnInhibit(bool success, std::optional<InhibitOperationResult> result);
  void AttemptUninhibit();
  void OnUninhibit(bool success);

  void CheckScanningIfNeeded();
  void CheckForScanningStopped();
  bool HasScanningStopped();
  void OnScanningChangeTimeout();

  void CheckInhibitPropertyIfNeeded();
  void CheckForInhibit();
  void CheckForUninhibit();
  void OnInhibitPropertyChangeTimeout();

  void PopRequestAndProcessNext();

  void SetInhibitProperty();
  void OnSetPropertySuccess();
  void OnSetPropertyError(bool attempted_inhibit,
                          const std::string& error_name);
  // Returns result of setting inhibit property. |result| is the operation
  // error result and is set only for failures.
  void ReturnSetInhibitPropertyResult(
      bool success,
      std::optional<InhibitOperationResult> result);

  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  raw_ptr<NetworkDeviceHandler> network_device_handler_ = nullptr;

  State state_ = State::kIdle;
  base::queue<std::unique_ptr<InhibitRequest>> inhibit_requests_;

  size_t uninhibit_attempts_so_far_ = 0;
  base::OneShotTimer set_inhibit_timer_;
  base::OneShotTimer scanning_change_timer_;

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<CellularInhibitor> weak_ptr_factory_{this};
};

std::ostream& COMPONENT_EXPORT(CHROMEOS_NETWORK) operator<<(
    std::ostream& stream,
    const ash::CellularInhibitor::State& state);

std::ostream& COMPONENT_EXPORT(CHROMEOS_NETWORK) operator<<(
    std::ostream& stream,
    const ash::CellularInhibitor::InhibitReason& inhibit_reason);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_INHIBITOR_H_
