// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_

#include <memory>
#include <string_view>

#include "components/browser_actuator/public/browser_actuator_service.h"

namespace browser_actuator {

class TransportChannelImpl;

class BrowserActuatorServiceImpl : public BrowserActuatorService {
 public:
  BrowserActuatorServiceImpl();
  ~BrowserActuatorServiceImpl() override;

  BrowserActuatorServiceImpl(const BrowserActuatorServiceImpl&) = delete;
  BrowserActuatorServiceImpl& operator=(const BrowserActuatorServiceImpl&) =
      delete;

  // BrowserActuatorService implementation.
  bool IsInitialized() const override;
  TransportChannel* GetChannel() override;
  TransportSession* GetOrCreateSession(std::string_view session_id) override;
  TransportSession* GetSession(std::string_view session_id) override;

 private:
  std::unique_ptr<TransportChannelImpl> channel_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_
