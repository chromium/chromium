// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/browser_credential_manager_factory.h"

#include "components/password_manager/core/browser/credential_manager_impl.h"

namespace password_manager {
BrowserCredentialManagerFactory::BrowserCredentialManagerFactory(
    PasswordManagerClient* client)
    : client_(client) {}

std::unique_ptr<credential_management::CredentialManagerInterface>
BrowserCredentialManagerFactory::CreateCredentialManager() {
  return std::make_unique<password_manager::CredentialManagerImpl>(client_);
}
}  // namespace password_manager
