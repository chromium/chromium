// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_ISOLATED_MODE_SCOPED_ISOLATED_MODE_OVERRIDE_FOR_TESTING_H_
#define COMPONENTS_ENTERPRISE_ISOLATED_MODE_SCOPED_ISOLATED_MODE_OVERRIDE_FOR_TESTING_H_

#include <optional>

#include "base/auto_reset.h"

namespace enterprise_isolated_mode {

// Scoped test helper to override
// `IsolatedModeSettingsService::ReplacesIncognito()` for services created while
// this object is in scope. Restores the previous override on destruction.
class [[maybe_unused, nodiscard]] ScopedIsolatedModeOverrideForTesting {
 public:
  explicit ScopedIsolatedModeOverrideForTesting(bool replaces_incognito = true);

  ScopedIsolatedModeOverrideForTesting(
      const ScopedIsolatedModeOverrideForTesting&) = delete;
  ScopedIsolatedModeOverrideForTesting& operator=(
      const ScopedIsolatedModeOverrideForTesting&) = delete;
  ~ScopedIsolatedModeOverrideForTesting();

 private:
  base::AutoReset<std::optional<bool>> default_override_;
};

}  // namespace enterprise_isolated_mode

#endif  // COMPONENTS_ENTERPRISE_ISOLATED_MODE_SCOPED_ISOLATED_MODE_OVERRIDE_FOR_TESTING_H_
