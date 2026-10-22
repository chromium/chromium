// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/public/browser_actuator_service.h"

namespace browser_actuator::transport {
class TransportService;
}  // namespace browser_actuator::transport

namespace browser_actuator {

class BrowserActuatorServiceImpl : public BrowserActuatorService {
 public:
  // `transport_service` is null when the transport is disabled. Otherwise it
  // must outlive this service; `BrowserActuatorServiceFactory` depends on
  // `TransportServiceFactory` to guarantee that.
  explicit BrowserActuatorServiceImpl(
      transport::TransportService* transport_service);
  ~BrowserActuatorServiceImpl() override;

  BrowserActuatorServiceImpl(const BrowserActuatorServiceImpl&) = delete;
  BrowserActuatorServiceImpl& operator=(const BrowserActuatorServiceImpl&) =
      delete;

  // BrowserActuatorService:
  bool IsInitialized() const override;
  transport::TransportChannel* GetChannel() override;
  transport::TransportSession* GetOrCreateSession(
      std::string_view session_id) override;
  transport::TransportSession* GetSession(std::string_view session_id) override;
  transport::SessionStreamRecorderFactory* GetStreamRecorderFactory() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<transport::TransportService> transport_service_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_
