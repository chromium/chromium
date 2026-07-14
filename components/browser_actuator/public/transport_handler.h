// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_

#include <string_view>

namespace browser_actuator {

// Interface that feature clients implement to receive messages for a specific
// PayloadType from the TransportChannel.
class TransportHandler {
 public:
  virtual ~TransportHandler() = default;

  // Process incoming downstream message.
  // TODO(crbug.com/532660606): Replace this raw payload with a structured
  // type once incoming payload protos are finalized.
  virtual void OnMessage(std::string_view payload) = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_
