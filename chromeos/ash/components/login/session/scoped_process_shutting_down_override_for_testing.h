// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_SESSION_SCOPED_PROCESS_SHUTTING_DOWN_OVERRIDE_FOR_TESTING_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_SESSION_SCOPED_PROCESS_SHUTTING_DOWN_OVERRIDE_FOR_TESTING_H_

namespace ash {

// Makes `IsProcessShuttingDown()` return true while this object is alive.
class ScopedProcessShuttingDownOverrideForTesting {
 public:
  ScopedProcessShuttingDownOverrideForTesting();
  ScopedProcessShuttingDownOverrideForTesting(
      const ScopedProcessShuttingDownOverrideForTesting&) = delete;
  ScopedProcessShuttingDownOverrideForTesting& operator=(
      const ScopedProcessShuttingDownOverrideForTesting&) = delete;
  ~ScopedProcessShuttingDownOverrideForTesting();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_SESSION_SCOPED_PROCESS_SHUTTING_DOWN_OVERRIDE_FOR_TESTING_H_
