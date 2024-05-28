// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_SERVICE_H_
#define CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"

namespace enterprise_companion {

// The core of the enterprise companion. All functions and callbacks must be
// called on the same sequence.
class EnterpriseCompanionService {
 public:
  virtual ~EnterpriseCompanionService() = default;

  virtual void Shutdown(base::OnceClosure callback) = 0;
};

std::unique_ptr<EnterpriseCompanionService> CreateEnterpriseCompanionService(
    base::OnceClosure shutdown_callback);

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_SERVICE_H_
