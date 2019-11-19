// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/shared_resource_scheduler.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {

namespace secure_channel {

namespace {

// Sorted from highest priority to lowest, to ensure that high-priority
// requests are retrieved from the scheduler first.
constexpr const ConnectionPriority kOrderedPriorities[] = {
    ConnectionPriority::kHigh, ConnectionPriority::kMedium,
    ConnectionPriority::kLow};

// Removes |item| from |list|; emits a crash if not in the list.
void RemoveItemFromList(const DeviceIdPair& item,
                        std::list<DeviceIdPair>* list) {
  for (auto it = list->begin(); it != list->end(); ++it) {
    if (*it != item)
      continue;

    list->erase(it);
    return;
  }

  PA_LOG(ERROR) << "RemoveItemFromList(): Tried to remove an item from |list|, "
                << "but that item was not present. Item: " << item;
  NOTREACHED();
}

// Remove the first item from |list| and returns it. If |list| is empty,
// base::nullopt is returned.
base::Optional<DeviceIdPair> RemoveFirstItemFromList(
    std::list<DeviceIdPair>* list) {
  if (list->empty())
    return base::nullopt;

  DeviceIdPair first_item = list->front();
  list->pop_front();
  return first_item;
}

}  // namespace

SharedResourceScheduler::SharedResourceScheduler() = default;

SharedResourceScheduler::~SharedResourceScheduler() = default;

void SharedResourceScheduler::ScheduleRequest(
    const DeviceIdPair& request,
    ConnectionPriority connection_priority) {
  if (base::Contains(request_to_priority_map_, request)) {
    PA_LOG(ERROR) << "SharedResourceScheduler::ScheduleRequest(): Tried to "
                  << "schedule a request which was already scheduled. Request: "
                  << request << ", Priority: " << connection_priority;
    NOTREACHED();
  }

  priority_to_queued_requests_map_[connection_priority].push_back(request);
  request_to_priority_map_[request] = connection_priority;
}

void SharedResourceScheduler::UpdateRequestPriority(
    const DeviceIdPair& request,
    ConnectionPriority connection_priority) {
  if (!base::Contains(request_to_priority_map_, request)) {
    PA_LOG(ERROR) << "SharedResourceScheduler::UpdateRequestPriority(): Tried "
                  << "to update priority for a request which was not "
                  << "scheduled. Request: " << request
                  << ", Priority: " << connection_priority;
    NOTREACHED();
  }

  if (request_to_priority_map_[request] == connection_priority) {
    PA_LOG(WARNING) << "SharedResourceScheduler::UpdateRequestPriority(): "
                    << "Tried update priority for a request, but the request "
                    << "was already at that priority.";
    return;
  }

  // Remove the item from the old list.
  RemoveItemFromList(
      request,
      &priority_to_queued_requests_map_[request_to_priority_map_[request]]);

  // Add it to the new list.
  priority_to_queued_requests_map_[connection_priority].push_back(request);

  // Update the priority map.
  request_to_priority_map_[request] = connection_priority;
}

void SharedResourceScheduler::RemoveScheduledRequest(
    const DeviceIdPair& request) {
  if (!base::Contains(request_to_priority_map_, request)) {
    PA_LOG(ERROR) << "SharedResourceScheduler::RemoveScheduledRequest(): Tried "
                  << "to remove a scheduled request, but that request was not "
                  << "actually scheduled. Request: " << request;
    NOTREACHED();
  }

  auto& list_for_priority =
      priority_to_queued_requests_map_[request_to_priority_map_[request]];

  // Remove from list in |priority_to_queued_requests_map_|.
  bool was_removed_from_list = false;
  for (auto it = list_for_priority.begin(); it != list_for_priority.end();
       ++it) {
    if (*it == request) {
      list_for_priority.erase(it);
      was_removed_from_list = true;
      break;
    }
  }

  if (!was_removed_from_list) {
    PA_LOG(ERROR) << "SharedResourceScheduler::RemoveScheduledRequest(): Tried "
                  << "to remove a scheduled request, but that request was not "
                  << "present in priority_to_queued_requests_map_. "
                  << "Request: " << request;
    NOTREACHED();
  }

  // Remove from |request_to_priority_map_|.
  size_t num_removed = request_to_priority_map_.erase(request);
  if (num_removed != 1u) {
    PA_LOG(ERROR) << "SharedResourceScheduler::RemoveScheduledRequest(): Tried "
                  << "to remove a scheduled request, but that request was not "
                  << "present in request_to_priority_map_. "
                  << "Request: " << request;
    NOTREACHED();
  }
}

base::Optional<std::pair<DeviceIdPair, ConnectionPriority>>
SharedResourceScheduler::GetNextScheduledRequest() {
  for (const auto& priority : kOrderedPriorities) {
    base::Optional<DeviceIdPair> potential_request =
        RemoveFirstItemFromList(&priority_to_queued_requests_map_[priority]);
    if (!potential_request)
      continue;

    size_t num_removed = request_to_priority_map_.erase(*potential_request);
    if (num_removed != 1u) {
      PA_LOG(ERROR) << "SharedResourceScheduler::GetNextScheduledRequest(): "
                    << "Tried to remove request from "
                    << "request_to_priority_map_, but no request was present."
                    << "Request: " << *potential_request;
      NOTREACHED();
    }

    return std::make_pair(*potential_request, priority);
  }

  return base::nullopt;
}

base::Optional<ConnectionPriority>
SharedResourceScheduler::GetHighestPriorityOfScheduledRequests() {
  for (const auto& priority : kOrderedPriorities) {
    if (!priority_to_queued_requests_map_[priority].empty())
      return priority;
  }

  return base::nullopt;
}

}  // namespace secure_channel

}  // namespace chromeos
