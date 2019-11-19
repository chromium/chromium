// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_advertiser_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/secure_channel/ble_service_data_helper.h"
#include "chromeos/services/secure_channel/error_tolerant_ble_advertisement_impl.h"
#include "chromeos/services/secure_channel/shared_resource_scheduler.h"
#include "chromeos/services/secure_channel/timer_factory.h"

namespace chromeos {

namespace secure_channel {

BleAdvertiserImpl::ActiveAdvertisementRequest::ActiveAdvertisementRequest(
    DeviceIdPair device_id_pair,
    ConnectionPriority connection_priority,
    std::unique_ptr<base::OneShotTimer> timer)
    : device_id_pair(device_id_pair),
      connection_priority(connection_priority),
      timer(std::move(timer)) {}

BleAdvertiserImpl::ActiveAdvertisementRequest::~ActiveAdvertisementRequest() =
    default;

// static
BleAdvertiserImpl::Factory* BleAdvertiserImpl::Factory::test_factory_ = nullptr;

// static
BleAdvertiserImpl::Factory* BleAdvertiserImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleAdvertiserImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

BleAdvertiserImpl::Factory::~Factory() = default;

// static
const int64_t BleAdvertiserImpl::kNumSecondsPerAdvertisementTimeslot = 10;

std::unique_ptr<BleAdvertiser> BleAdvertiserImpl::Factory::BuildInstance(
    Delegate* delegate,
    BleServiceDataHelper* ble_service_data_helper,
    BleSynchronizerBase* ble_synchronizer_base,
    TimerFactory* timer_factory,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
  return base::WrapUnique(new BleAdvertiserImpl(
      delegate, ble_service_data_helper, ble_synchronizer_base, timer_factory,
      sequenced_task_runner));
}

BleAdvertiserImpl::BleAdvertiserImpl(
    Delegate* delegate,
    BleServiceDataHelper* ble_service_data_helper,
    BleSynchronizerBase* ble_synchronizer_base,
    TimerFactory* timer_factory,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : BleAdvertiser(delegate),
      ble_service_data_helper_(ble_service_data_helper),
      ble_synchronizer_base_(ble_synchronizer_base),
      timer_factory_(timer_factory),
      sequenced_task_runner_(sequenced_task_runner),
      shared_resource_scheduler_(std::make_unique<SharedResourceScheduler>()) {}

BleAdvertiserImpl::~BleAdvertiserImpl() = default;

void BleAdvertiserImpl::AddAdvertisementRequest(
    const DeviceIdPair& request,
    ConnectionPriority connection_priority) {
  requests_already_removed_due_to_failed_advertisement_.erase(request);

  if (base::Contains(all_requests_, request)) {
    PA_LOG(ERROR) << "BleAdvertiserImpl::AddAdvertisementRequest(): Tried to "
                  << "add advertisement request which was already present. "
                  << "Request: " << request
                  << ", Priority: " << connection_priority;
    NOTREACHED();
  }
  all_requests_.insert(request);

  shared_resource_scheduler_->ScheduleRequest(request, connection_priority);

  // If an existing request is active but has a lower priority than
  // |connection_priority|, that request should be replaced by |request|.
  bool was_replaced =
      ReplaceLowPriorityAdvertisementIfPossible(connection_priority);
  if (was_replaced)
    return;

  UpdateAdvertisementState();
}

void BleAdvertiserImpl::UpdateAdvertisementRequestPriority(
    const DeviceIdPair& request,
    ConnectionPriority connection_priority) {
  if (base::Contains(requests_already_removed_due_to_failed_advertisement_,
                     request))
    return;

  if (!base::Contains(all_requests_, request)) {
    PA_LOG(ERROR) << "BleAdvertiserImpl::UpdateAdvertisementRequestPriority(): "
                  << "Tried to update request priority for a request, but that "
                  << "request was not present. Request: " << request
                  << ", Priority: " << connection_priority;
    NOTREACHED();
  }

  base::Optional<size_t> index_for_active_request =
      GetIndexForActiveRequest(request);

  if (!index_for_active_request) {
    // If the request is not currently active, update its priority in the
    // scheduler.
    shared_resource_scheduler_->UpdateRequestPriority(request,
                                                      connection_priority);

    // If an existing request is active but has a lower priority than
    // |connection_priority|, that request should be replaced by |request|.
    ReplaceLowPriorityAdvertisementIfPossible(connection_priority);
    return;
  }

  std::unique_ptr<ActiveAdvertisementRequest>& active_request =
      active_advertisement_requests_[*index_for_active_request];

  // If there is an active advertisement and no pending advertisements, update
  // the active advertisement priority and return.
  if (shared_resource_scheduler_->empty()) {
    active_request->connection_priority = connection_priority;
    return;
  }

  // If there is an active advertisement and the new priority of the request
  // is still at least as high as the highest priority of all pending
  // requests, update the active advertisement priority and return.
  if (connection_priority >=
      *shared_resource_scheduler_->GetHighestPriorityOfScheduledRequests()) {
    active_request->connection_priority = connection_priority;
    return;
  }

  // The active advertisement's priority has been reduced, and it is now lower
  // than the priority of at least one pending request. Thus, stop the existing
  // advertisement and reschedule the request for later.
  StopAdvertisementRequestAndUpdateActiveRequests(
      *index_for_active_request,
      true /* replaced_by_higher_priority_advertisement */,
      false /* was_removed */);
}

void BleAdvertiserImpl::RemoveAdvertisementRequest(
    const DeviceIdPair& request) {
  // If the request has already been deleted, then this was invoked by a failure
  // callback following a failure to generate an advertisement.
  auto it = requests_already_removed_due_to_failed_advertisement_.find(request);
  if (it != requests_already_removed_due_to_failed_advertisement_.end()) {
    requests_already_removed_due_to_failed_advertisement_.erase(it);
    return;
  }

  if (!base::Contains(all_requests_, request)) {
    PA_LOG(ERROR) << "BleAdvertiserImpl::RemoveAdvertisementRequest(): Tried "
                  << "to remove an advertisement request, but that request was "
                  << "not present. Request: " << request;
    NOTREACHED();
  }
  all_requests_.erase(request);

  base::Optional<size_t> index_for_active_request =
      GetIndexForActiveRequest(request);

  // If the request is not currently active, remove it from the scheduler and
  // return.
  if (!index_for_active_request) {
    shared_resource_scheduler_->RemoveScheduledRequest(request);
    return;
  }

  // The active advertisement should be stopped and not rescheduled.
  StopAdvertisementRequestAndUpdateActiveRequests(
      *index_for_active_request,
      false /* replaced_by_higher_priority_advertisement */,
      true /* was_removed */);
}

bool BleAdvertiserImpl::ReplaceLowPriorityAdvertisementIfPossible(
    ConnectionPriority connection_priority) {
  base::Optional<size_t> index_with_lower_priority =
      GetIndexWithLowerPriority(connection_priority);
  if (!index_with_lower_priority)
    return false;

  StopAdvertisementRequestAndUpdateActiveRequests(
      *index_with_lower_priority,
      true /* replaced_by_higher_priority_advertisement */,
      false /* was_removed */);

  return true;
}

base::Optional<size_t> BleAdvertiserImpl::GetIndexWithLowerPriority(
    ConnectionPriority connection_priority) {
  ConnectionPriority lowest_priority = ConnectionPriority::kHigh;
  base::Optional<size_t> index_with_lowest_priority;

  // Loop through |active_advertisement_requests_|, searching for the entry with
  // the lowest priority.
  for (size_t i = 0; i < active_advertisement_requests_.size(); ++i) {
    if (!active_advertisement_requests_[i])
      continue;

    if (active_advertisement_requests_[i]->connection_priority <
        lowest_priority) {
      lowest_priority = active_advertisement_requests_[i]->connection_priority;
      index_with_lowest_priority = i;
    }
  }

  // If |index_with_lowest_priority| was never set, all active advertisement
  // requests have high priority, so they should not be replaced with the new
  // connection attempt.
  if (!index_with_lowest_priority)
    return base::nullopt;

  // If the lowest priority in |active_advertisement_requests_| is at least as
  // high as |connection_priority|, this slot shouldn't be replaced with the
  // new connection attempt.
  if (lowest_priority >= connection_priority)
    return base::nullopt;

  return *index_with_lowest_priority;
}

void BleAdvertiserImpl::UpdateAdvertisementState() {
  for (size_t i = 0; i < active_advertisement_requests_.size(); ++i) {
    // If there are any empty slots in |active_advertisement_requests_| and
    // |shared_resource_scheduler_| contains pending requests, remove the
    // pending request and make it active.
    if (!active_advertisement_requests_[i] &&
        !shared_resource_scheduler_->empty()) {
      AddActiveAdvertisementRequest(i);
    }

    // If there are any empty slots in |active_advertisements_| and
    // |active_advertisement_requests_| contains a request for an advertisement,
    // generate a new active advertisement.
    if (active_advertisement_requests_[i] && !active_advertisements_[i])
      AttemptToAddActiveAdvertisement(i);
  }
}

void BleAdvertiserImpl::AddActiveAdvertisementRequest(size_t index_to_add) {
  // Retrieve the next request from the scheduler.
  std::pair<DeviceIdPair, ConnectionPriority> request_with_priority =
      *shared_resource_scheduler_->GetNextScheduledRequest();

  // Create a timer, and have it go off in kNumSecondsPerAdvertisementTimeslot
  // seconds.
  std::unique_ptr<base::OneShotTimer> timer =
      timer_factory_->CreateOneShotTimer();
  timer->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kNumSecondsPerAdvertisementTimeslot),
      base::Bind(
          &BleAdvertiserImpl::StopAdvertisementRequestAndUpdateActiveRequests,
          base::Unretained(this), index_to_add,
          false /* replaced_by_higher_priority_advertisement */,
          false /* was_removed */));

  active_advertisement_requests_[index_to_add] =
      std::make_unique<ActiveAdvertisementRequest>(request_with_priority.first,
                                                   request_with_priority.second,
                                                   std::move(timer));
}

void BleAdvertiserImpl::AttemptToAddActiveAdvertisement(size_t index_to_add) {
  const DeviceIdPair pair =
      active_advertisement_requests_[index_to_add]->device_id_pair;
  std::unique_ptr<DataWithTimestamp> service_data =
      ble_service_data_helper_->GenerateForegroundAdvertisement(pair);

  // If an advertisement could not be created, the request is immediately
  // removed. It's also tracked to prevent future operations from referencing
  // the removed request.
  if (!service_data) {
    RemoveAdvertisementRequest(pair);
    requests_already_removed_due_to_failed_advertisement_.insert(pair);

    // Schedules AttemptToNotifyFailureToGenerateAdvertisement() to run
    // after the tasks in the current sequence. This is done to avoid invoking
    // an advertisement generation failure callback on the same call stack that
    // added the advertisement request in the first place.
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BleAdvertiserImpl::AttemptToNotifyFailureToGenerateAdvertisement,
            weak_factory_.GetWeakPtr(), pair));

    return;
  }

  active_advertisements_[index_to_add] =
      ErrorTolerantBleAdvertisementImpl::Factory::Get()->BuildInstance(
          pair, std::move(service_data), ble_synchronizer_base_);
}

base::Optional<size_t> BleAdvertiserImpl::GetIndexForActiveRequest(
    const DeviceIdPair& request) {
  for (size_t i = 0; i < active_advertisement_requests_.size(); ++i) {
    auto& active_request = active_advertisement_requests_[i];
    if (active_request && active_request->device_id_pair == request)
      return i;
  }

  return base::nullopt;
}

void BleAdvertiserImpl::StopAdvertisementRequestAndUpdateActiveRequests(
    size_t index,
    bool replaced_by_higher_priority_advertisement,
    bool was_removed) {
  // Stop the actual advertisement at this index, if there is one.
  StopActiveAdvertisement(index);

  // Make a copy of the request to stop from |active_advertisement_requests_|,
  // then reset the original version which resided in the array.
  std::unique_ptr<ActiveAdvertisementRequest> request_to_stop =
      std::move(active_advertisement_requests_[index]);

  // If the request was not removed by a client, this request is being stopped
  // either due to a timeout or due to a higher-priority request taking its
  // spot. In these two cases, the request should be rescheduled, and the
  // delegate should be notified that the timeslot ended.
  if (!was_removed) {
    shared_resource_scheduler_->ScheduleRequest(
        request_to_stop->device_id_pair, request_to_stop->connection_priority);
    NotifyAdvertisingSlotEnded(request_to_stop->device_id_pair,
                               replaced_by_higher_priority_advertisement);
  }

  UpdateAdvertisementState();
}

void BleAdvertiserImpl::StopActiveAdvertisement(size_t index) {
  auto& active_advertisement = active_advertisements_[index];
  if (!active_advertisement)
    return;

  // If |active_advertisement| is already in the process of stopping, there is
  // nothing to do.
  if (active_advertisement->HasBeenStopped())
    return;

  active_advertisement->Stop(
      base::Bind(&BleAdvertiserImpl::OnActiveAdvertisementStopped,
                 base::Unretained(this), index));
}

void BleAdvertiserImpl::OnActiveAdvertisementStopped(size_t index) {
  active_advertisements_[index].reset();
  UpdateAdvertisementState();
}

void BleAdvertiserImpl::AttemptToNotifyFailureToGenerateAdvertisement(
    const DeviceIdPair& device_id_pair) {
  // If the request is not found, then that request has either been removed
  // again or re-scheduled after it failed to generate an advertisement, but
  // before this task could execute.
  if (!base::Contains(requests_already_removed_due_to_failed_advertisement_,
                      device_id_pair)) {
    return;
  }

  NotifyFailureToGenerateAdvertisement(device_id_pair);
}

}  // namespace secure_channel

}  // namespace chromeos
