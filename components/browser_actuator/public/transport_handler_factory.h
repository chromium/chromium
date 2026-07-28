// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_FACTORY_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_FACTORY_H_

#include <memory>
#include <vector>

#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_handler.h"

namespace browser_actuator {

class TransportSession;

// Factory interface that features implement to receive transport messages for
// a specific PayloadType from the TransportChannel.
class TransportHandlerFactory {
 public:
  virtual ~TransportHandlerFactory() = default;

  // Enum ID for the factory, used to ensure we do not re-register factories of
  // the same type
  virtual FactoryId GetFactoryId() const = 0;

  // Self-reports the payload types handled by this feature.
  virtual std::vector<PayloadType> GetSupportedPayloadTypes() const = 0;

  // Called when a new session starts.
  // The returned unique_ptr is owned by the session.
  virtual std::unique_ptr<TransportHandler> OnNewSession(
      TransportSession* session) = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_FACTORY_H_
