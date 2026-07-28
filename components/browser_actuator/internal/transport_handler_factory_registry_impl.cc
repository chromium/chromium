// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_handler_factory_registry_impl.h"

#include <algorithm>
#include <ostream>

#include "base/check.h"
#include "components/browser_actuator/public/transport_handler_factory.h"

namespace browser_actuator {

TransportHandlerFactoryRegistryImpl::TransportHandlerFactoryRegistryImpl() =
    default;

TransportHandlerFactoryRegistryImpl::~TransportHandlerFactoryRegistryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TransportHandlerFactoryRegistryImpl::RegisterFactory(
    TransportHandlerFactory* factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(factory);
  for (const auto& [type, list] : factories_) {
    for (TransportHandlerFactory* registered : list) {
      if (registered->GetFactoryId() == factory->GetFactoryId() &&
          registered != factory) {
        DCHECK(false) << "Duplicate registration of factory with ID: "
                      << static_cast<int>(factory->GetFactoryId());
        return;
      }
    }
  }
  for (PayloadType payload_type : factory->GetSupportedPayloadTypes()) {
    auto& registered_factories = factories_[payload_type];
    if (!std::ranges::contains(registered_factories, factory)) {
      registered_factories.push_back(factory);
    }
  }
}

void TransportHandlerFactoryRegistryImpl::UnregisterFactory(
    TransportHandlerFactory* factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(factory);
  for (PayloadType payload_type : factory->GetSupportedPayloadTypes()) {
    auto it = factories_.find(payload_type);
    if (it != factories_.end()) {
      auto& registered_factories = it->second;
      std::erase(registered_factories, factory);
      if (registered_factories.empty()) {
        factories_.erase(it);
      }
    }
  }
}

std::vector<TransportHandlerFactory*>
TransportHandlerFactoryRegistryImpl::GetFactories(
    PayloadType payload_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = factories_.find(payload_type);
  if (it == factories_.end()) {
    return {};
  }
  return std::vector<TransportHandlerFactory*>(it->second.begin(),
                                               it->second.end());
}

void TransportHandlerFactoryRegistryImpl::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  factories_.clear();
}

bool TransportHandlerFactoryRegistryImpl::IsEmpty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return factories_.empty();
}

}  // namespace browser_actuator
