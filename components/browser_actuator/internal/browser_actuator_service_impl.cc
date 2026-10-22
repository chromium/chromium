// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include "components/browser_actuator/public/transport/transport_service.h"

namespace browser_actuator {

BrowserActuatorServiceImpl::BrowserActuatorServiceImpl(
    transport::TransportService* transport_service)
    : transport_service_(transport_service) {}

BrowserActuatorServiceImpl::~BrowserActuatorServiceImpl() = default;

bool BrowserActuatorServiceImpl::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

// TODO(crbug.com/568448841): The methods below forward to TransportService
// until every caller uses TransportServiceFactory directly.

transport::TransportChannel* BrowserActuatorServiceImpl::GetChannel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return transport_service_ ? transport_service_->GetChannel() : nullptr;
}

transport::TransportSession* BrowserActuatorServiceImpl::GetOrCreateSession(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return transport_service_ ? transport_service_->GetOrCreateSession(session_id)
                            : nullptr;
}

transport::TransportSession* BrowserActuatorServiceImpl::GetSession(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return transport_service_ ? transport_service_->GetSession(session_id)
                            : nullptr;
}

transport::SessionStreamRecorderFactory*
BrowserActuatorServiceImpl::GetStreamRecorderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return transport_service_ ? transport_service_->GetStreamRecorderFactory()
                            : nullptr;
}

}  // namespace browser_actuator
