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
  using const_iterator =
      base::circular_deque<PermissionRequest*>::const_iterator;
  using const_reverse_iterator =
      base::circular_deque<PermissionRequest*>::const_reverse_iterator;

  // Not copyable or movable
  PermissionRequestQueue(const PermissionRequestQueue&) = delete;
  PermissionRequestQueue& operator=(const PermissionRequestQueue&) = delete;
  PermissionRequestQueue();
  ~PermissionRequestQueue();

  bool IsEmpty() const;
  size_t Count() const;
  size_t Count(PermissionRequest* request) const;

  // Push a new request into queue. When |reorder_based_on_priority| is set, the
  // request might be inserted to correct position based on its priority,
  // instead of be pushed to the front of queue.
  void Push(permissions::PermissionRequest* request,
            bool reorder_based_on_priority = false);
  PermissionRequest* Pop();
  PermissionRequest* Peek() const;

  // Searches queued_requests_ and returns the first matching request, or
  // nullptr if there is no match.
  PermissionRequest* FindDuplicate(PermissionRequest* request) const;

  // Used for iterating over the queued requests.
  const_iterator begin() const;
  const_iterator end() const;

 private:
  void PushInternal(permissions::PermissionRequest* request);

  base::circular_deque<PermissionRequest*> queued_requests_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_QUEUE_H_
