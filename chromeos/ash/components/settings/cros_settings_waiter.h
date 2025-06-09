// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_WAITER_H_
#define CHROMEOS_ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_WAITER_H_

#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/run_loop.h"

namespace base {
class CallbackListSubscription;
}  // namespace base

namespace ash {

class CrosSettingsWaiter {
 public:
  // Sets up the waiter to wait for cros settings specified by `settings`.
  // If multiple settings are specified, it stops waiting when any of them
  // is updated.
  // Global ash::CrosSettings instance must be initialized on creation.
  explicit CrosSettingsWaiter(base::span<const std::string_view> settings);
  CrosSettingsWaiter(const CrosSettingsWaiter&) = delete;
  CrosSettingsWaiter& operator=(const CrosSettingsWaiter&) = delete;
  ~CrosSettingsWaiter();

  // Waits for the update of the settings passed to the ctor.
  // Returns true on success.
  void Wait();

 private:
  base::RunLoop run_loop_;
  std::vector<base::CallbackListSubscription> subscriptions_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_WAITER_H_
