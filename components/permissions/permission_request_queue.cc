// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_queue.h"

#include "base/ranges/algorithm.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_util.h"

namespace permissions {

PermissionRequestQueue::PermissionRequestQueue()
    : queued_requests_(static_cast<size_t>(Priority::kNum)) {}

PermissionRequestQueue::~PermissionRequestQueue() = default;

bool PermissionRequestQueue::IsEmpty() const {
  return !size_;
}

size_t PermissionRequestQueue::Count(PermissionRequest* request) const {
  size_t count = 0;
  for (const auto& request_list : queued_requests_) {
    count += base::ranges::count(request_list, request);
  }
  return count;
}

void PermissionRequestQueue::Push(PermissionRequest* request) {
  Priority priority = DetermineRequestPriority(request);

  // High priority requests are always pushed to the back since they don't use
  // the chip.
  if (priority == Priority::kHigh) {
    PushBackInternal(request, priority);
    return;
  }

  // If the platform does not support the chip, push to the back.
  if (!PermissionUtil::DoesPlatformSupportChip()) {
    PushBackInternal(request, priority);
    return;
  }

  // Otherwise push to the front since chip requests use FILO ordering.
  PushFrontInternal(request, priority);
}

void PermissionRequestQueue::PushFront(
    permissions::PermissionRequest* request) {
  Priority priority = DetermineRequestPriority(request);
  PushFrontInternal(request, priority);
}

void PermissionRequestQueue::PushBack(permissions::PermissionRequest* request) {
  Priority priority = DetermineRequestPriority(request);
  PushBackInternal(request, priority);
}

PermissionRequest* PermissionRequestQueue::Pop() {
  std::vector<base::circular_deque<PermissionRequest*>>::reverse_iterator it;
  CHECK(!IsEmpty());
  // Skip entries that contain empty queues.
  for (it = queued_requests_.rbegin();
       it != queued_requests_.rend() && it->empty(); ++it) {
  }
  CHECK(it != queued_requests_.rend());
  PermissionRequest* front = it->front();
  it->pop_front();
  --size_;
  return front;
}

PermissionRequest* PermissionRequestQueue::Peek() const {
  CHECK(!IsEmpty());
  std::vector<base::circular_deque<PermissionRequest*>>::const_reverse_iterator
      it;
  // Skip entries that contain empty queues.
  for (it = queued_requests_.rbegin();
       it != queued_requests_.rend() && it->empty(); ++it) {
  }
  CHECK(it != queued_requests_.rend());
  return it->front();
}

PermissionRequest* PermissionRequestQueue::FindDuplicate(
    PermissionRequest* request) const {
  auto priority = DetermineRequestPriority(request);
  const auto& queued_request_list =
      queued_requests_[static_cast<size_t>(priority)];
  for (PermissionRequest* queued_request : queued_request_list) {
    if (request->IsDuplicateOf(queued_request)) {
      return queued_request;
    }
  }
  return nullptr;
}

PermissionRequestQueue::const_iterator PermissionRequestQueue::begin() const {
  return queued_requests_.begin();
}

PermissionRequestQueue::const_iterator PermissionRequestQueue::end() const {
  return queued_requests_.end();
}

// static
PermissionRequestQueue::Priority
PermissionRequestQueue::DetermineRequestPriority(
    permissions::PermissionRequest* request) {
  if (request->IsEmbeddedPermissionElementInitiated()) {
    return Priority::kHigh;
  }

  if (permissions::PermissionUtil::DoesPlatformSupportChip() &&
      permissions::PermissionUtil::IsLowPriorityPermissionRequest(request)) {
    return Priority::kLow;
  }

  return Priority::kMedium;
}

void PermissionRequestQueue::PushFrontInternal(
    permissions::PermissionRequest* request,
    Priority priority) {
  queued_requests_[static_cast<size_t>(priority)].push_front(request);
  ++size_;
}

void PermissionRequestQueue::PushBackInternal(
    permissions::PermissionRequest* request,
    Priority priority) {
  queued_requests_[static_cast<size_t>(priority)].push_back(request);
  ++size_;
}

}  // namespace permissions
