// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_FACTORY_REGISTRY_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_FACTORY_REGISTRY_H_

#include <vector>

#include "components/browser_actuator/public/common.h"

namespace browser_actuator {

class TransportHandlerFactory;

// Registry of the TransportHandlerFactory instances that can create handlers
// for a TransportChannel.
//
// TODO(crbug.com/537014928): Add support for Observers to allow ongoing
// sessions to detect and initialize factories registered after channel
// establishment.
class TransportHandlerFactoryRegistry {
 public:
  virtual ~TransportHandlerFactoryRegistry() = default;

  // Registers a factory. Registry does NOT take ownership;
  // ownership remains with the feature client.
  virtual void RegisterFactory(TransportHandlerFactory* factory) = 0;
  virtual void UnregisterFactory(TransportHandlerFactory* factory) = 0;

  // Retrieves all factories registered for a payload type.
  virtual std::vector<TransportHandlerFactory*> GetFactories(
      PayloadType payload_type) const = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_FACTORY_REGISTRY_H_
