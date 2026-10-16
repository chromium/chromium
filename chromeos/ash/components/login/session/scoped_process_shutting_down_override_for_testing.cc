// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/session/scoped_process_shutting_down_override_for_testing.h"

#include "base/check.h"
#include "components/keep_alive_registry/keep_alive_registry.h"

namespace ash {

ScopedProcessShuttingDownOverrideForTesting::
    ScopedProcessShuttingDownOverrideForTesting() {
  KeepAliveRegistry* registry = KeepAliveRegistry::GetInstance();
  CHECK(!registry->IsShuttingDown());
  registry->SetIsShuttingDown(true);
}

ScopedProcessShuttingDownOverrideForTesting::
    ~ScopedProcessShuttingDownOverrideForTesting() {
  KeepAliveRegistry::GetInstance()->SetIsShuttingDown(false);
}

}  // namespace ash
