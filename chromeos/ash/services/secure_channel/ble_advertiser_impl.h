// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISER_IMPL_H_

#include <array>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/secure_channel/ble_advertiser.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/ble_constants.h"

namespace ash::timer_factory {
class TimerFactory;
}  // namespace ash::timer_factory

namespace base {
class OneShotTimer;
}

namespace ash::secure_channel {

class BleSynchronizerBase;
class BluetoothHelper;
class ErrorTolerantBleAdvertisement;
class SharedResourceScheduler;
enum class ConnectionPriority;

// Concrete BleAdvertiser implementation. Because systems have a limited number
// of BLE advertisement slots, this class limits the number of concurrent
// advertisements to kMaxConcurrentAdvertisements.
//
// This class tracks two types of requests: active requests (i.e., ones who are
// scheduled to be advertising) and queued requests (i.e., ones who are waiting
// for their turn to use a BLE advertisement slot). A request with a higher
// priority is always given an active advertising slot before a class with a
// lower priority. For equal priorities, a round-robin algorithm is used.
//
// An active advertisement remains active until it is removed by the client,
// pre-empted by another request with a higher priority, or until its timeslot
// ends. This class provides kNumSecondsPerAdvertisementTimeslot seconds for
// each timeslot. When a timeslot ends or when a request is replaced by a
// higher-priority request, the delegate is notified. The delegate is not
// notified when a device is explicitly removed.
class BleAdvertiserImpl : public BleAdvertiser {
 public:
  class Factory {
   public:
    static std::unique_ptr<BleAdvertiser> Create(
        Delegate* delegate,
        BluetoothHelper* bluetooth_helper,
        BleSynchronizerBase* ble_synchronizer_base,
        ash::timer_factory::TimerFactory* timer_factory,
        scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
            base::SequencedTaskRunner::GetCurrentDefault());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<BleAdvertiser> CreateInstance(
        Delegate* delegate,
        BluetoothHelper* bluetooth_helper,
        BleSynchronizerBase* ble_synchronizer_base,
        ash::timer_factory::TimerFactory* timer_factory,
        scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) = 0;

   private:
    static Factory* test_factory_;
  };

  BleAdvertiserImpl(const BleAdvertiserImpl&) = delete;
  BleAdvertiserImpl& operator=(const BleAdvertiserImpl&) = delete;

  ~BleAdvertiserImpl() override;

 private:
  friend class SecureChannelBleAdvertiserImplTest;

  struct ActiveAdvertisementRequest {
    ActiveAdvertisementRequest(DeviceIdPair device_id_pair,
                               ConnectionPriority connection_priority,
                               std::unique_ptr<base::OneShotTimer> timer);

    ActiveAdvertisementRequest(const ActiveAdvertisementRequest&) = delete;
    ActiveAdvertisementRequest& operator=(const ActiveAdvertisementRequest&) =
        delete;

    virtual ~ActiveAdvertisementRequest();

    DeviceIdPair device_id_pair;
    ConnectionPriority connection_priority;
    std::unique_ptr<base::OneShotTimer> timer;
  };

  static const int64_t kNumSecondsPerAdvertisementTimeslot;

  BleAdvertiserImpl(
      Delegate* delegate,
      BluetoothHelper* bluetooth_helper,
      BleSynchronizerBase* ble_synchronizer_base,
      ash::timer_factory::TimerFactory* timer_factory,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  // BleAdvertiser:
  void AddAdvertisementRequest(const DeviceIdPair& request,
                               ConnectionPriority connection_priority) override;
  void UpdateAdvertisementRequestPriority(
      const DeviceIdPair& request,
      ConnectionPriority connection_priority) override;
  void RemoveAdvertisementRequest(const DeviceIdPair& request) override;

  bool ReplaceLowPriorityAdvertisementIfPossible(
      ConnectionPriority connection_priority);
  std::optional<size_t> GetIndexWithLowerPriority(
      ConnectionPriority connection_priority);
  void UpdateAdvertisementState();
  void AddActiveAdvertisementRequest(size_t index_to_add);
  void AttemptToAddActiveAdvertisement(size_t index_to_add);
  std::optional<size_t> GetIndexForActiveRequest(const DeviceIdPair& request);
  void StopAdvertisementRequestAndUpdateActiveRequests(
      size_t index,
      bool replaced_by_higher_priority_advertisement,
      bool should_reschedule);
  void StopActiveAdvertisement(size_t index);
  void OnActiveAdvertisementStopped(size_t index);

  // Notifies the delegate of a request's failure to generate an advertisement,
  // unless the failed request has already been processed and removed from
  // |requests_already_removed_due_to_failed_advertisement_|.
  void AttemptToNotifyFailureToGenerateAdvertisement(
      const DeviceIdPair& device_id_pair);

  raw_ptr<BluetoothHelper> bluetooth_helper_;
  raw_ptr<BleSynchronizerBase> ble_synchronizer_base_;
  raw_ptr<ash::timer_factory::TimerFactory> timer_factory_;

  // For posting tasks to the current base::SequencedTaskRunner.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  std::unique_ptr<SharedResourceScheduler> shared_resource_scheduler_;
  DeviceIdPairSet all_requests_;

  // An array of length kMaxConcurrentAdvertisements, whose elements correspond
  // to the active BLE advertisement requests. Elements in this array are
  // scheduled to be advertising at this time. However, because stopping
  // advertisements is an asynchronous operation, the active requests may not
  // necessarily correspond to the active advertisements.
  std::array<std::unique_ptr<ActiveAdvertisementRequest>,
             kMaxConcurrentAdvertisements>
      active_advertisement_requests_;

  // An array of length kMaxConcurrentAdvertisements whose elements correspond
  // to the active BLE advertisements.
  std::array<std::unique_ptr<ErrorTolerantBleAdvertisement>,
             kMaxConcurrentAdvertisements>
      active_advertisements_;

  // If a request fails to generate an advertisement, it is immediately removed
  // internally and tracked here. Then, when the delegate failure callback tries
  // to clean up the failed advertisement (or something else tries to re-add or
  // remove it again), its associated entry in this set will be removed instead.
  base::flat_set<DeviceIdPair>
      requests_already_removed_due_to_failed_advertisement_;

  base::WeakPtrFactory<BleAdvertiserImpl> weak_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_ADVERTISER_IMPL_H_
