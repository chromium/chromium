// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/logging/stub_log_manager.h"

namespace autofill {

void StubLogManager::OnLogRouterAvailabilityChanged(bool router_can_be_used) {}

void StubLogManager::SetSuspended(bool suspended) {}

bool StubLogManager::IsLoggingActive() const {
  return true;
}

LogBufferSubmitter StubLogManager::Log() {
  return LogBufferSubmitter(this);
}

void StubLogManager::ProcessLog(base::Value::Dict node,
                                base::PassKey<LogBufferSubmitter>) {}

}  // namespace autofill
