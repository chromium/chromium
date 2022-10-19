// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_queue.h"

#include "base/ranges/algorithm.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"

namespace permissions {

PermissionRequestQueue::PermissionRequestQueue() = default;

PermissionRequestQueue::~PermissionRequestQueue() = default;

bool PermissionRequestQueue::IsEmpty() const {
  return queued_requests_.empty();
}

size_t PermissionRequestQueue::Count() const {
  return queued_requests_.size();
}

size_t PermissionRequestQueue::Count(PermissionRequest* request) const {
  return base::ranges::count(queued_requests_, request);
}

void PermissionRequestQueue::PushInternal(PermissionRequest* request) {
  if (base::FeatureList::IsEnabled(features::kPermissionQuietChip) &&
      !base::FeatureList::IsEnabled(features::kPermissionChip)) {
    queued_requests_.push_front(request);
  } else {
    queued_requests_.push_back(request);
  }
}

void PermissionRequestQueue::Push(PermissionRequest* request,
                                  bool reorder_based_on_priority) {
  if (!reorder_based_on_priority) {
    PushInternal(request);
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kPermissionQuietChip) ||
      !base::FeatureList::IsEnabled(features::kPermissionChip)) {
    PushInternal(request);
    return;
  }

  // There're situations we need to take the priority into consideration (eg:
  // kPermissionChip and kPermissionQuietChip both are enabled). In such cases,
  // push the new request to front of queue if it has high priority. Otherwise,
  // insert the request after the first low priority request.
  // Note that, the queue processing order is FIFO, but we have to iterate the
  // queue in reverse order (see |PushInternal|)
  if (queued_requests_.empty() ||
      !PermissionUtil::IsLowPriorityPermissionRequest(request)) {
    PushInternal(request);
    return;
  }

  PermissionRequestQueue::const_reverse_iterator iter =
      queued_requests_.rbegin();
  for (; iter != queued_requests_.rend() &&
         !PermissionUtil::IsLowPriorityPermissionRequest(*iter);
       ++iter) {
  }

  queued_requests_.insert(iter.base(), request);
}

PermissionRequest* PermissionRequestQueue::Pop() {
  PermissionRequest* next = Peek();
  if (base::FeatureList::IsEnabled(features::kPermissionChip))
    queued_requests_.pop_back();
  else
    queued_requests_.pop_front();
  return next;
}

PermissionRequest* PermissionRequestQueue::Peek() const {
  return base::FeatureList::IsEnabled(features::kPermissionChip)
             ? queued_requests_.back()
             : queued_requests_.front();
}

PermissionRequest* PermissionRequestQueue::FindDuplicate(
    PermissionRequest* request) const {
  for (PermissionRequest* queued_request : queued_requests_) {
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

}  // namespace permissions
