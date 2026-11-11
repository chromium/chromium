// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_HANDLER_FACTORY_REGISTRY_H_
#define COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_HANDLER_FACTORY_REGISTRY_H_

#include <vector>

#include "base/observer_list_types.h"
#include "components/stream_transport/public/common.h"

namespace stream_transport {

class TransportHandlerFactory;

// Registry of the TransportHandlerFactory instances that can create handlers
// for a TransportChannel.
//
// TODO(crbug.com/537014928): Add support for Observers to allow ongoing
// sessions to detect and initialize factories registered after channel
// establishment.
class TransportHandlerFactoryRegistry {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when a registered factory is unregistered from the registry.
    virtual void OnFactoryUnregistered(FactoryId factory_id) {}

    // Called when the registry is being destroyed.
    virtual void OnRegistryDestroyed() {}
  };

  virtual ~TransportHandlerFactoryRegistry() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Registers a factory. Registry does NOT take ownership;
  // ownership remains with the feature client.
  virtual void RegisterFactory(TransportHandlerFactory* factory) = 0;
  virtual void UnregisterFactory(TransportHandlerFactory* factory) = 0;

  // Retrieves all factories registered for a payload type.
  virtual std::vector<TransportHandlerFactory*> GetFactories(
      PayloadType payload_type) const = 0;
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_HANDLER_FACTORY_REGISTRY_H_
