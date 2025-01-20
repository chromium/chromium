// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORDS_PROVIDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORDS_PROVIDER_H_

#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

// Interface for an object holding saved credentials and providing
// them to clients via a getter.
class PasswordsProvider {
 public:
  PasswordsProvider() = default;
  virtual ~PasswordsProvider() = default;

  virtual std::vector<CredentialUIEntry> GetSavedCredentials() const = 0;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORDS_PROVIDER_H_
