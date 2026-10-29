// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_ISOLATED_MODE_SCOPED_ISOLATED_MODE_OVERRIDE_FOR_TESTING_H_
#define COMPONENTS_ENTERPRISE_ISOLATED_MODE_SCOPED_ISOLATED_MODE_OVERRIDE_FOR_TESTING_H_

#include <optional>

#include "base/auto_reset.h"

namespace enterprise_isolated_mode {

class IsolatedModeSettingsService;

// Scoped test helper to override
// `IsolatedModeSettingsService::ReplacesIncognito()`.
//
// Two modes are supported:
// 1. Default override (for services created while this object is in scope):
//      ScopedIsolatedModeOverrideForTesting scoped_isolated_mode;
// 2. Per-instance override (mutates a specific `IsolatedModeSettingsService`
//    instance and restores its previous value when going out of scope;
//    `service` must outlive this object):
//      ScopedIsolatedModeOverrideForTesting scoped_isolated_mode(&service);
class [[maybe_unused, nodiscard]] ScopedIsolatedModeOverrideForTesting {
 public:
  explicit ScopedIsolatedModeOverrideForTesting(bool replaces_incognito = true);
  explicit ScopedIsolatedModeOverrideForTesting(
      IsolatedModeSettingsService* service,
      bool replaces_incognito = true);

  // Prevent other pointer types from implicitly converting to `bool`.
  template <typename T>
  explicit ScopedIsolatedModeOverrideForTesting(T*) = delete;

  ScopedIsolatedModeOverrideForTesting(
      const ScopedIsolatedModeOverrideForTesting&) = delete;
  ScopedIsolatedModeOverrideForTesting& operator=(
      const ScopedIsolatedModeOverrideForTesting&) = delete;
  ~ScopedIsolatedModeOverrideForTesting();

 private:
  std::optional<base::AutoReset<std::optional<bool>>> default_override_;
  std::optional<base::AutoReset<bool>> service_override_;
};

}  // namespace enterprise_isolated_mode

#endif  // COMPONENTS_ENTERPRISE_ISOLATED_MODE_SCOPED_ISOLATED_MODE_OVERRIDE_FOR_TESTING_H_
