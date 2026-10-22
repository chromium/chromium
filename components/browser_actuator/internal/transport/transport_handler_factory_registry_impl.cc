// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/transport_handler_factory_registry_impl.h"

#include <algorithm>
#include <ostream>

#include "base/check.h"
#include "components/browser_actuator/public/transport/transport_handler_factory.h"

namespace browser_actuator::transport {

TransportHandlerFactoryRegistryImpl::TransportHandlerFactoryRegistryImpl() =
    default;

TransportHandlerFactoryRegistryImpl::~TransportHandlerFactoryRegistryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnRegistryDestroyed();
  }
}

void TransportHandlerFactoryRegistryImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void TransportHandlerFactoryRegistryImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
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
  bool removed = false;
  for (PayloadType payload_type : factory->GetSupportedPayloadTypes()) {
    auto it = factories_.find(payload_type);
    if (it != factories_.end()) {
      auto& registered_factories = it->second;
      if (std::erase(registered_factories, factory) > 0) {
        removed = true;
      }
      if (registered_factories.empty()) {
        factories_.erase(it);
      }
    }
  }
  if (removed) {
    const FactoryId factory_id = factory->GetFactoryId();
    for (Observer& observer : observers_) {
      observer.OnFactoryUnregistered(factory_id);
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

}  // namespace browser_actuator::transport
