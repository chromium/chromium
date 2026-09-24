// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/common.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace browser_actuator {

class SessionStreamRecorderFactory;
class TransportChannelImpl;
class TransportHandlerFactory;

class BrowserActuatorServiceImpl : public BrowserActuatorService {
 public:
  // `extra_factories` are transport handler factories that this service takes
  // ownership of and registers with the channel's handler factory registry.
  // This lets embedder-layer code (for example //chrome) install factories
  // without //components needing to know their concrete types.
  BrowserActuatorServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      std::vector<std::unique_ptr<TransportHandlerFactory>> extra_factories);
  ~BrowserActuatorServiceImpl() override;

  BrowserActuatorServiceImpl(const BrowserActuatorServiceImpl&) = delete;
  BrowserActuatorServiceImpl& operator=(const BrowserActuatorServiceImpl&) =
      delete;

  // BrowserActuatorService implementation.
  bool IsInitialized() const override;
  TransportChannel* GetChannel() override;
  TransportSession* GetOrCreateSession(std::string_view session_id) override;
  TransportSession* GetSession(std::string_view session_id) override;

  // Exposes owned factories. Intended for embedder diagnostic consumers
  // (e.g. chrome://browser-actuator-internals) to retrieve concrete factories
  // by `id`.
  TransportHandlerFactory* GetFactory(FactoryId id) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Declared before `channel_` on purpose. C++ destroys members in reverse
  // declaration order, so `channel_` -- and the factory registry it owns, which
  // holds raw pointers into `extra_factories_` and
  // `session_stream_recorder_factory_` -- is torn down first.
  std::vector<std::unique_ptr<TransportHandlerFactory>> extra_factories_;
  std::unique_ptr<SessionStreamRecorderFactory>
      session_stream_recorder_factory_;

  std::unique_ptr<TransportChannelImpl> channel_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_
