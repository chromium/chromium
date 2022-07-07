// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_queue.h"

#include "base/stl_util.h"
#include "components/permissions/features.h"

namespace permissions {

PermissionRequestQueue::PermissionRequestQueue() = default;

PermissionRequestQueue::~PermissionRequestQueue() = default;

bool PermissionRequestQueue::IsEmpty() {
  return queued_requests_.empty();
}

size_t PermissionRequestQueue::Count() {
  return queued_requests_.size();
}

size_t PermissionRequestQueue::Count(PermissionRequest* request) {
  return base::STLCount(queued_requests_, request);
}

void PermissionRequestQueue::Push(permissions::PermissionRequest* request) {
  if (base::FeatureList::IsEnabled(features::kPermissionQuietChip) &&
      !base::FeatureList::IsEnabled(features::kPermissionChip)) {
    queued_requests_.push_front(request);
  } else {
    queued_requests_.push_back(request);
  }
}

PermissionRequest* PermissionRequestQueue::Pop() {
  PermissionRequest* next = Peek();
  if (base::FeatureList::IsEnabled(features::kPermissionChip))
    queued_requests_.pop_back();
  else
    queued_requests_.pop_front();
  return next;
}

PermissionRequest* PermissionRequestQueue::Peek() {
  return base::FeatureList::IsEnabled(features::kPermissionChip)
             ? queued_requests_.back()
             : queued_requests_.front();
}

PermissionRequest* PermissionRequestQueue::FindDuplicate(
    PermissionRequest* request) {
  for (PermissionRequest* queued_request : queued_requests_) {
    if (request->IsDuplicateOf(queued_request)) {
      return queued_request;
    }
  }
  return nullptr;
}

PermissionRequestQueue::iterator PermissionRequestQueue::begin() {
  return queued_requests_.begin();
}

PermissionRequestQueue::iterator PermissionRequestQueue::end() {
  return queued_requests_.end();
}

};  // namespace permissions
