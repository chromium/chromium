// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_PROVIDER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_PROVIDER_INTERFACE_H_

#include <memory>
#include <vector>

#include "components/password_manager/core/browser/password_form_forward.h"

namespace password_manager {

// This is a delegate of the ExportFlow interface used to retrieve exportable
// passwords.
// TODO(1047726): Merge this interface with SavedPasswordsPresenter.
class CredentialProviderInterface {
 public:
  // Gets all password entries.
  virtual std::vector<std::unique_ptr<PasswordForm>> GetAllPasswords() = 0;

 protected:
  virtual ~CredentialProviderInterface() {}
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_PROVIDER_INTERFACE_H_
