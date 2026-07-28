// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_HANDLER_FACTORY_REGISTRY_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_HANDLER_FACTORY_REGISTRY_IMPL_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_handler_factory_registry.h"

namespace browser_actuator {

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

  void RegisterFactory(TransportHandlerFactory* factory) override;
  void UnregisterFactory(TransportHandlerFactory* factory) override;
  std::vector<TransportHandlerFactory*> GetFactories(
      PayloadType payload_type) const override;

  void Clear();
  bool IsEmpty() const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::flat_map<PayloadType, std::vector<raw_ptr<TransportHandlerFactory>>>
      factories_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_HANDLER_FACTORY_REGISTRY_IMPL_H_
