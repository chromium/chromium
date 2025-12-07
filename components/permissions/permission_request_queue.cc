// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_queue.h"

#include <algorithm>

#include "components/permissions/permission_request.h"
#include "components/permissions/permission_util.h"

namespace permissions {

PermissionRequestQueue::PermissionRequestQueue()
    : queued_requests_(static_cast<size_t>(Priority::kNum)) {}

PermissionRequestQueue::~PermissionRequestQueue() = default;

bool PermissionRequestQueue::IsEmpty() const {
  return !size_;
}

bool PermissionRequestQueue::Contains(PermissionRequest* request) const {
  for (const auto& request_list : queued_requests_) {
    if (std::ranges::any_of(request_list,
                            [=](const auto& request_list_element) {
                              return request_list_element.get() == request;
                            })) {
      return true;
    }
  }
  return false;
}

void PermissionRequestQueue::Push(
    std::unique_ptr<permissions::PermissionRequest> request) {
  Priority priority = DetermineRequestPriority(request.get());

  // High priority requests are always pushed to the back since they don't use
  // the chip.
  if (priority == Priority::kHigh) {
    PushBackInternal(std::move(request), priority);
    return;
  }

  // If the platform does not support the chip, push to the back.
  if (!PermissionUtil::DoesPlatformSupportChip()) {
    PushBackInternal(std::move(request), priority);
    return;
  }

  // Otherwise push to the front since chip requests use FILO ordering.
  PushFrontInternal(std::move(request), priority);
}

void PermissionRequestQueue::PushFront(
    std::unique_ptr<permissions::PermissionRequest> request) {
  Priority priority = DetermineRequestPriority(request.get());
  PushFrontInternal(std::move(request), priority);
}

void PermissionRequestQueue::PushBack(
    std::unique_ptr<permissions::PermissionRequest> request) {
  Priority priority = DetermineRequestPriority(request.get());
  PushBackInternal(std::move(request), priority);
}

std::unique_ptr<permissions::PermissionRequest> PermissionRequestQueue::Pop() {
  std::vector<base::circular_deque<
      std::unique_ptr<permissions::PermissionRequest>>>::reverse_iterator it;
  CHECK(!IsEmpty());
  // Skip entries that contain empty queues.
  for (it = queued_requests_.rbegin();
       it != queued_requests_.rend() && it->empty(); ++it) {
  }
  CHECK(it != queued_requests_.rend());
  std::unique_ptr<permissions::PermissionRequest> front =
      std::move(it->front());
  it->pop_front();
  --size_;
  return front;
}

PermissionRequest* PermissionRequestQueue::Peek() const {
  CHECK(!IsEmpty());
  std::vector<base::circular_deque<
      std::unique_ptr<permissions::PermissionRequest>>>::const_reverse_iterator
      it;
  // Skip entries that contain empty queues.
  for (it = queued_requests_.rbegin();
       it != queued_requests_.rend() && it->empty(); ++it) {
  }
  CHECK(it != queued_requests_.rend());
  return it->front().get();
}

PermissionRequest* PermissionRequestQueue::FindDuplicate(
    PermissionRequest* request) const {
  auto priority = DetermineRequestPriority(request);
  const auto& queued_request_list =
      queued_requests_[static_cast<size_t>(priority)];
  for (const auto& queued_request : queued_request_list) {
    if (request->IsDuplicateOf(queued_request.get())) {
      return queued_request.get();
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
    std::unique_ptr<permissions::PermissionRequest> request,
    Priority priority) {
  queued_requests_[static_cast<size_t>(priority)].push_front(
      std::move(request));
  ++size_;
}

void PermissionRequestQueue::PushBackInternal(
    std::unique_ptr<permissions::PermissionRequest> request,
    Priority priority) {
  queued_requests_[static_cast<size_t>(priority)].push_back(std::move(request));
  ++size_;
}

}  // namespace permissions
