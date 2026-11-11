// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_INTERNAL_TRANSPORT_HANDLER_FACTORY_REGISTRY_IMPL_H_
#define COMPONENTS_STREAM_TRANSPORT_INTERNAL_TRANSPORT_HANDLER_FACTORY_REGISTRY_IMPL_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_handler_factory_registry.h"

namespace stream_transport {

class TransportHandlerFactory;

// Registry to control the lifecycles of the HandlerFactories for a
// TransportChannel
class TransportHandlerFactoryRegistryImpl
    : public TransportHandlerFactoryRegistry {
 public:
  TransportHandlerFactoryRegistryImpl();
  ~TransportHandlerFactoryRegistryImpl() override;

  TransportHandlerFactoryRegistryImpl(
      const TransportHandlerFactoryRegistryImpl&) = delete;
  TransportHandlerFactoryRegistryImpl& operator=(
      const TransportHandlerFactoryRegistryImpl&) = delete;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void RegisterFactory(TransportHandlerFactory* factory) override;
  void UnregisterFactory(TransportHandlerFactory* factory) override;
  std::vector<TransportHandlerFactory*> GetFactories(
      PayloadType payload_type) const override;

  void Clear();
  bool IsEmpty() const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::flat_map<PayloadType, std::vector<raw_ptr<TransportHandlerFactory>>>
      factories_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_INTERNAL_TRANSPORT_HANDLER_FACTORY_REGISTRY_IMPL_H_
