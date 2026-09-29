// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PASSWORD_MANAGER_PASSWORD_REUSE_MANAGER_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_PASSWORD_MANAGER_PASSWORD_REUSE_MANAGER_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace password_manager {
class PasswordReuseManager;
}  // namespace password_manager

namespace ash {

// Provides the password_manager::PasswordReuseManager associated with a user to
// ChromeOS callers without forcing them to depend on
// //chrome/browser/password_manager's Profile-keyed
// PasswordReuseManagerFactory. The concrete implementation lives in //chrome
// (see //chrome/browser/ash/browser_delegate/keyed_service_provider/
// password_reuse_manager_provider_impl.h).
class COMPONENT_EXPORT(PASSWORD_REUSE_MANAGER_PROVIDER)
    PasswordReuseManagerProvider {
 public:
  PasswordReuseManagerProvider();
  PasswordReuseManagerProvider(const PasswordReuseManagerProvider&) = delete;
  PasswordReuseManagerProvider& operator=(const PasswordReuseManagerProvider&) =
      delete;
  virtual ~PasswordReuseManagerProvider();

  // Returns the process-wide singleton.
  static PasswordReuseManagerProvider& Get();

  // Returns the PasswordReuseManager associated with `account_id`, or nullptr
  // if none is available. The returned pointer is owned by the
  // BrowserContext-keyed service infrastructure; callers must not delete it.
  virtual password_manager::PasswordReuseManager* Find(
      const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PASSWORD_MANAGER_PASSWORD_REUSE_MANAGER_PROVIDER_H_
