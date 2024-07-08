// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_SERVICE_H_
#define CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/enterprise_companion/dm_client.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"

namespace enterprise_companion {

class EventLoggerManager;

// The core of the Enterprise Companion App. All functions and callbacks must be
// called on the same sequence.
class EnterpriseCompanionService {
 public:
  virtual ~EnterpriseCompanionService() = default;

  virtual void Shutdown(base::OnceClosure callback) = 0;

  virtual void FetchPolicies(StatusCallback callback) = 0;
};

std::unique_ptr<EnterpriseCompanionService> CreateEnterpriseCompanionService(
    std::unique_ptr<DMClient> dm_client,
    std::unique_ptr<EventLoggerManager> event_logger_manager,
    base::OnceClosure shutdown_callback);

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_SERVICE_H_
