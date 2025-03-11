// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CREDENTIAL_MANAGEMENT_CREDENTIAL_MANAGER_FACTORY_INTERFACE_H_
#define COMPONENTS_CREDENTIAL_MANAGEMENT_CREDENTIAL_MANAGER_FACTORY_INTERFACE_H_

#include <memory>

namespace password_manager {
class CredentialManagerInterface;
}  // namespace password_manager

namespace credential_management {

// Interface for factory classes that create implementations of
// `CredentialManagerInterface`.
class CredentialManagerFactoryInterface {
 public:
  virtual ~CredentialManagerFactoryInterface() = default;
  virtual std::unique_ptr<password_manager::CredentialManagerInterface>
  CreateCredentialManager() = 0;
};
}  // namespace credential_management
#endif  // COMPONENTS_CREDENTIAL_MANAGEMENT_CREDENTIAL_MANAGER_FACTORY_INTERFACE_H_
