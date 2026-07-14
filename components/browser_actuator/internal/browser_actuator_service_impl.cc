// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

namespace browser_actuator {

BrowserActuatorServiceImpl::BrowserActuatorServiceImpl() = default;

BrowserActuatorServiceImpl::~BrowserActuatorServiceImpl() = default;

bool BrowserActuatorServiceImpl::IsInitialized() const {
  return true;
}

TransportChannel* BrowserActuatorServiceImpl::GetChannel() {
  // TODO(crbug.com/532660606): Implement this getter when the
  // TransportChannel is implemented.
  return nullptr;
}

}  // namespace browser_actuator
