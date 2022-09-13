// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_QUEUE_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_QUEUE_H_

#include <cstddef>

#include "base/containers/circular_deque.h"
#include "components/permissions/permission_request.h"

namespace permissions {

// Provides a container for holding pending PermissionRequest objects and
// provides access methods respecting the currently applicable feature flag
// configuration.
class PermissionRequestQueue {
 public:
  using iterator = base::circular_deque<PermissionRequest*>::iterator;

  // Not copyable or movable
  PermissionRequestQueue(const PermissionRequestQueue&) = delete;
  PermissionRequestQueue& operator=(const PermissionRequestQueue&) = delete;
  PermissionRequestQueue();
  ~PermissionRequestQueue();

  bool IsEmpty();
  size_t Count();
  size_t Count(PermissionRequest* request);
  void Push(permissions::PermissionRequest* request);
  PermissionRequest* Pop();
  PermissionRequest* Peek();

  // Searches queued_requests_ and returns the first matching request, or
  // nullptr if there is no match.
  PermissionRequest* FindDuplicate(PermissionRequest* request);

 private:
  iterator begin();
  iterator end();

  base::circular_deque<PermissionRequest*> queued_requests_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_QUEUE_H_
