// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_DELEGATE_H_

#include <set>
#include <string>

#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/policy_types.h"

namespace device_signals {

// Delegate representing the user that is currently logged-in to the browser
// itself.
class UserDelegate {
 public:
  virtual ~UserDelegate() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns true if the browser is currently executing in the context of the
  // ChromeOS sign-in screen.
  virtual bool IsSigninContext() const = 0;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Returns true if the current browser user is managed by an organization that
  // is affiliated with the organization managing the device.
  virtual bool IsAffiliated() const = 0;

  // Returns true if the current browser user is managed.
  virtual bool IsManagedUser() const = 0;

  // Returns true if `gaia_id` represents the same user as the one currently
  // logged-in to the browser.
  virtual bool IsSameUser(const std::string& gaia_id) const = 0;

  // Returns the currently enabled Scopes for policies known to require signals.
  virtual std::set<policy::PolicyScope> GetPolicyScopesNeedingSignals()
      const = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_DELEGATE_H_
