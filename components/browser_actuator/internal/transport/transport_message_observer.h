// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TRANSPORT_MESSAGE_OBSERVER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TRANSPORT_MESSAGE_OBSERVER_H_

#include <string_view>

#include "base/observer_list_types.h"

namespace browser_actuator {
class ActuatorDownstreamMessage;
class ActuatorUpstreamMessage;
}  // namespace browser_actuator

namespace browser_actuator::transport {

// Watches whole transport envelopes. Diagnostic consumers implement
// this; it is not part of message delivery.
class TransportMessageObserver : public base::CheckedObserver {
 public:
  // Called right before a downstream message reaches any `TransportHandler`.
  // On the WatchSessions stream path this is once per envelope, in sequence
  // order. On the out-of-band FCM wake-up path it is once per payload, as a
  // single-payload envelope with `sequence_number` 0. Messages that the
  // session drops before dispatch (e.g. duplicates rejected by the sequencer)
  // are not reported.
  virtual void OnDownstreamMessage(
      std::string_view session_id,
      const ActuatorDownstreamMessage& message) = 0;

  virtual void OnUpstreamMessage(std::string_view session_id,
                                 const ActuatorUpstreamMessage& message) = 0;
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TRANSPORT_MESSAGE_OBSERVER_H_
