// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_QUEUE_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_QUEUE_H_

#include <cstddef>
#include <vector>

#include "base/containers/circular_deque.h"
#include "components/permissions/permission_request.h"

namespace permissions {

// Provides a container for holding pending PermissionRequest objects and
// provides access methods respecting the currently applicable feature flag
// configuration.
// The queue of permission requests is always held in the order of:
// High Priority Requests > Normal Priority Requests > Low Priority Requests.
// Using the |PushFront| and |PushBack| functions will push the new request in
// the front or back of the section of the queue that corresponds to that
// request's priority.

// High Priority Requests are requests that come from an Page-Embedded
// Permission Control.
// Low Priority Requests are requests for non-urgent permission types
// (notifications, geolocation) if the current platform supports the permission
// chip. If the permission chip is not supported, there are no low priority
// requests.
// Normal Priority Requests are all other requests.
class PermissionRequestQueue {
 public:
  using const_iterator =
      std::vector<base::circular_deque<PermissionRequest*>>::const_iterator;

  // Not copyable or movable
  PermissionRequestQueue(const PermissionRequestQueue&) = delete;
  PermissionRequestQueue& operator=(const PermissionRequestQueue&) = delete;
  PermissionRequestQueue();
  ~PermissionRequestQueue();

  bool IsEmpty() const;
  size_t Count(PermissionRequest* request) const;
  size_t size() const { return size_; }

  // Push a new request into queue. This function will decide based on request
  // priority and platform whether to call |PushBack| or |PushFront|.
  void Push(permissions::PermissionRequest* request);

  // Push a new request into the front of the section of the queue that
  // corresponds to its priority. E.g.: calling this function on a normal
  // priority |request| will put it in front of any other normal priority
  // requests, but still behind any high priority requests.
  void PushFront(permissions::PermissionRequest* request);

  // Push a new request into the back of the section of the queue that
  // corresponds to its priority. E.g.: calling this function on a normal
  // priority |request| will put it behind any other normal priority requests,
  // but still in front of any low priority requests.
  void PushBack(permissions::PermissionRequest* request);

  PermissionRequest* Pop();
  PermissionRequest* Peek() const;

  // Searches queued_requests_ and returns the first matching request, or
  // nullptr if there is no match.
  PermissionRequest* FindDuplicate(PermissionRequest* request) const;

  const_iterator begin() const;
  const_iterator end() const;

 private:
  enum class Priority {
    kLow,
    kMedium,
    kHigh,

    // Used to set the correct size of the |queued_requests_| vector.
    kNum,
  };

  static Priority DetermineRequestPriority(
      permissions::PermissionRequest* request);

  void PushFrontInternal(permissions::PermissionRequest* request,
                         Priority priority);
  void PushBackInternal(permissions::PermissionRequest* request,
                        Priority priority);

  // Each priority has a separate deque. There is an assumption made that the
  // priorities have strictly ascending, contignous values from lowest to
  // highest.
  std::vector<base::circular_deque<PermissionRequest*>> queued_requests_;

  size_t size_{0};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_REQUEST_QUEUE_H_
