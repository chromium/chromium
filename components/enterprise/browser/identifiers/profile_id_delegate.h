// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_PROFILE_ID_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_PROFILE_ID_DELEGATE_H_

#include <string>

namespace enterprise {

// Delegate for methods used in the profile ID service.
class ProfileIdDelegate {
 public:
  virtual ~ProfileIdDelegate() = default;

  // Gets the device ID (also known as the virtual device ID) and returns it.
  virtual std::string GetDeviceId() = 0;
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_PROFILE_ID_DELEGATE_H_
