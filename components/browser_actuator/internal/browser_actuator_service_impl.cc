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

}  // namespace browser_actuator
