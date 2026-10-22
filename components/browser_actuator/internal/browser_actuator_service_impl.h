// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_

#include <memory>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/public/browser_actuator_service.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace browser_actuator::transport {
class SessionStreamRecorderFactory;
class TransportChannelImpl;
}  // namespace browser_actuator::transport

namespace browser_actuator {

class BrowserActuatorServiceImpl : public BrowserActuatorService {
 public:
  BrowserActuatorServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  ~BrowserActuatorServiceImpl() override;

  BrowserActuatorServiceImpl(const BrowserActuatorServiceImpl&) = delete;
  BrowserActuatorServiceImpl& operator=(const BrowserActuatorServiceImpl&) =
      delete;

  // BrowserActuatorService implementation.
  bool IsInitialized() const override;
  transport::TransportChannel* GetChannel() override;
  transport::TransportSession* GetOrCreateSession(
      std::string_view session_id) override;
  transport::TransportSession* GetSession(std::string_view session_id) override;
  transport::SessionStreamRecorderFactory* GetStreamRecorderFactory() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Declared before `channel_` on purpose. C++ destroys members in reverse
  // declaration order, so `channel_` -- and the factory registry it owns, which
  // holds a raw pointer to `session_stream_recorder_factory_` -- is torn down
  // first.
  std::unique_ptr<transport::SessionStreamRecorderFactory>
      session_stream_recorder_factory_;

  std::unique_ptr<transport::TransportChannelImpl> channel_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_
