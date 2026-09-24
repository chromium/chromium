// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_MESSAGE_OBSERVER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_MESSAGE_OBSERVER_H_

#include <string_view>

#include "base/observer_list_types.h"

namespace browser_actuator {

class ActuatorUpstreamMessage;

// Watches whole transport envelopes. Diagnostic consumers implement
// this; it is not part of message delivery.
//
// Only the upstream direction is observable today. Downstream
// messages already reach handlers through TransportHandler, and
// adding a second downstream path here would record each message
// twice. See crbug.com/565078376.
class TransportMessageObserver : public base::CheckedObserver {
 public:
  virtual void OnUpstreamMessage(std::string_view session_id,
                                 const ActuatorUpstreamMessage& message) = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_MESSAGE_OBSERVER_H_
