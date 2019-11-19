// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_STUB_LOG_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_STUB_LOG_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "components/autofill/core/browser/logging/log_manager.h"

namespace autofill {

// Use this in tests only, to provide a no-op implementation of LogManager.
class StubLogManager : public LogManager {
 public:
  StubLogManager() = default;
  ~StubLogManager() override = default;

 private:
  // LogManager
  void OnLogRouterAvailabilityChanged(bool router_can_be_used) override;
  void SetSuspended(bool suspended) override;
  void LogTextMessage(const std::string& text) const override;
  void LogEntry(base::Value&& entry) const override;
  bool IsLoggingActive() const override;
  LogBufferSubmitter Log() override;

  DISALLOW_COPY_AND_ASSIGN(StubLogManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_STUB_LOG_MANAGER_H_
