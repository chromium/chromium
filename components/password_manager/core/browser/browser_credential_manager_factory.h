// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BROWSER_CREDENTIAL_MANAGER_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BROWSER_CREDENTIAL_MANAGER_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "components/credential_management/credential_manager_factory_interface.h"
#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {
class PasswordManagerClient;

// The factory is used to create an instance of `CredentialManagerImpl` that's
// used by `ContentCredentialManager` to implement Credential Management API
// methods. This factory is browser-specific and doesn't exist on WebView.
class BrowserCredentialManagerFactory
    : public credential_management::CredentialManagerFactoryInterface {
 public:
  explicit BrowserCredentialManagerFactory(PasswordManagerClient* client);
  BrowserCredentialManagerFactory(const BrowserCredentialManagerFactory&) =
      delete;
  BrowserCredentialManagerFactory& operator=(
      const BrowserCredentialManagerFactory&) = delete;

  std::unique_ptr<credential_management::CredentialManagerInterface>
  CreateCredentialManager() override;

 private:
  raw_ptr<PasswordManagerClient> client_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BROWSER_CREDENTIAL_MANAGER_FACTORY_H_
