// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_DELEGATE_H_

struct AccountInfo;

namespace device_signals {

// Delegate representing the user that is currently logged into the browser
// itself.
class UserDelegate {
 public:
  virtual ~UserDelegate() = default;

  // Returns true if the current user is managed by an organization that is
  // affiliated with the organization managing the device.
  virtual bool IsAffiliated() const = 0;

  // Returns true if the user currently logged into the browser is managed and
  // is the same user as `user`.
  virtual bool IsSameManagedUser(const AccountInfo& user) const = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_DELEGATE_H_
