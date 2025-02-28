// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_FACTORY_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_FACTORY_INTERFACE_H_

#include <memory>

#include "components/password_manager/core/browser/credential_manager_interface.h"

namespace password_manager {
// Interface for factory classes that create implementations of
// `CredentialManagerInterface`.
class CredentialManagerFactoryInterface {
 public:
  virtual ~CredentialManagerFactoryInterface() = default;
  virtual std::unique_ptr<CredentialManagerInterface>
  CreateCredentialManager() = 0;
};
}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_FACTORY_INTERFACE_H_
