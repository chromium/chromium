// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_BROWSER_SUPPORT_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_BROWSER_SUPPORT_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/component_export.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"

namespace policy {
class PolicyMap;
}  // namespace policy

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash::standalone_browser {

// Class encapsulating the state of Lacros browser support.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
    BrowserSupport {
 public:
  BrowserSupport(const BrowserSupport&) = delete;
  BrowserSupport& operator=(const BrowserSupport&) = delete;

  // Initializes the global instance of BrowserSupport for the Primary User.
  static void InitializeForPrimaryUser(const policy::PolicyMap& policy_map,
                                       bool is_new_profile,
                                       bool is_regular_profile);

  // Destroys the global instance of BrowserSupport.
  static void Shutdown();

  // Returns true if BrowserSupport instance is initialized for the Primary
  // User.
  static bool IsInitializedForPrimaryUser();

  // Returns the global instance of BrowserSupport for the Primary User.
  static BrowserSupport* GetForPrimaryUser();

  // Returns whether CPU of this device is capable to run standalone browser.
  // Can be called even before Initialize() is called.
  static bool IsCpuSupported();

  // Directly sets the value to be returned by IsCpuSupported for testing.
  // Setting nullopt unsets the overridden behavior of IsCpuSupported.
  static void SetCpuSupportedForTesting(std::optional<bool> value);

  // Returns true if the standalone browser is allowed to be enabled.
  bool IsAllowed() const { return is_allowed_; }

  // Temporarily exposing internal function for transition period.
  // TODO(crbug.com/40286020): Hide the function along with refactoring.
  static bool IsEnabledInternal(const user_manager::User* user,
                                LacrosAvailability lacros_availability,
                                bool check_migration_status);

 private:
  BrowserSupport(bool is_allowed);
  ~BrowserSupport();

  const bool is_allowed_;
};

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_BROWSER_SUPPORT_H_
