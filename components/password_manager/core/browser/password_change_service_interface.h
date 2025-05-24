// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SERVICE_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SERVICE_INTERFACE_H_

#include "base/functional/callback_forward.h"

class GURL;

namespace password_manager {

// Abstract interface for high level interaction related to password change.
class PasswordChangeServiceInterface {
 public:
  // Checks whether current user is eligible to use password change.
  virtual bool IsPasswordChangeAvailable() = 0;

  // Checks whether password change is eligible for a given `url`.
  virtual bool IsPasswordChangeSupported(const GURL& url) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SERVICE_INTERFACE_H_
