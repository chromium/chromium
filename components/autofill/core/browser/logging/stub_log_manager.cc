// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/stub_log_manager.h"

namespace autofill {

void StubLogManager::OnLogRouterAvailabilityChanged(bool router_can_be_used) {}

void StubLogManager::SetSuspended(bool suspended) {}

void StubLogManager::LogTextMessage(const std::string& text) const {}

void StubLogManager::LogEntry(base::Value&& entry) const {}

bool StubLogManager::IsLoggingActive() const {
  return false;
}

LogBufferSubmitter StubLogManager::Log() {
  return LogBufferSubmitter(nullptr, false);
}

}  // namespace autofill
