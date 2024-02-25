// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_EVENT_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_EVENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"

namespace guest_view {

class GuestViewBase;

// A GuestViewEvent is a wrapper class for a GuestView event.
// GuestViewEvents may be queued until the guest is attached to a container.
// This wrapper class holds all the necessary information to fire the event
// on attachment. GuestViewEvents are owned by GuestViewBase.
class GuestViewEvent {
 public:
  GuestViewEvent(const std::string& name, base::Value::Dict args);

  GuestViewEvent(const GuestViewEvent&) = delete;
  GuestViewEvent& operator=(const GuestViewEvent&) = delete;

  ~GuestViewEvent();

  // This method will dispatch the event to the specified |guest|'s embedder and
  // use the provided |instance_id| for routing.
  void Dispatch(GuestViewBase* guest, int instance_id);

private:
  const std::string name_;
  std::optional<base::Value::Dict> args_;
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_EVENT_H_
