// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_FORM_CONVERTERS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_FORM_CONVERTERS_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"

namespace password_manager {

// Converts a StoredCredential to a PasswordForm.
// Passes by value to support efficient moves.
PasswordForm ToPasswordForm(const StoredCredential& cred);
PasswordForm ToPasswordForm(StoredCredential&& cred);

// Converts a PasswordForm to a StoredCredential.
// Passes by value to support efficient moves.
StoredCredential FromPasswordForm(PasswordForm form);

// Returns a deep copy of StoredCredential.
StoredCredential CloneStoredCredential(const StoredCredential& cred);

// Converts a vector of StoredCredentials to a vector of PasswordForms.
std::vector<PasswordForm> ToPasswordForms(
    std::vector<StoredCredential>&& credentials);
std::vector<PasswordForm> ToPasswordForms(
    const std::vector<StoredCredential>& credentials);

// Converts a vector of PasswordForms to a vector of StoredCredentials.
std::vector<StoredCredential> FromPasswordForms(
    std::vector<PasswordForm> forms);

// Converts BackendLoginsResultOrError to LoginsResultOrError.
LoginsResultOrError ToLoginsResultOrError(BackendLoginsResultOrError result);

// Helper to adapt LoginsResultOrError to BackendLoginsOrErrorReply.
inline base::OnceCallback<void(LoginsResultOrError)> AdaptLoginsResultCallback(
    BackendLoginsOrErrorReply callback) {
  return base::BindOnce(
      [](BackendLoginsOrErrorReply callback, LoginsResultOrError result) {
        if (std::holds_alternative<PasswordStoreBackendError>(result)) {
          std::move(callback).Run(std::get<PasswordStoreBackendError>(result));
        } else {
          std::move(callback).Run(
              FromPasswordForms(std::get<LoginsResult>(std::move(result))));
        }
      },
      std::move(callback));
}

// Helper to adapt BackendLoginsResultOrError to LoginsOrErrorReply.
inline BackendLoginsOrErrorReply AdaptBackendLoginsResultCallback(
    base::OnceCallback<void(LoginsResultOrError)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(LoginsResultOrError)> callback,
         BackendLoginsResultOrError result) {
        if (std::holds_alternative<PasswordStoreBackendError>(result)) {
          std::move(callback).Run(std::get<PasswordStoreBackendError>(result));
        } else {
          std::move(callback).Run(ToPasswordForms(
              std::get<BackendLoginsResult>(std::move(result))));
        }
      },
      std::move(callback));
}

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_FORM_CONVERTERS_H_
