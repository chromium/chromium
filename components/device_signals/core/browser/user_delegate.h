// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_DELEGATE_H_

#include <string>

namespace device_signals {

// Delegate representing the user that is currently logged-in to the browser
// itself.
class UserDelegate {
 public:
  virtual ~UserDelegate() = default;

  // Returns true if the current browser user is managed by an organization that
  // is affiliated with the organization managing the device.
  virtual bool IsAffiliated() const = 0;

  // Returns true if the current browser user is managed.
  virtual bool IsManaged() const = 0;

  // Returns true if `gaia_id` represents the same user as the one currently
  // logged-in to the browser.
  virtual bool IsSameUser(const std::string& gaia_id) const = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_DELEGATE_H_
