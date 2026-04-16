// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_FORM_CONVERTERS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_FORM_CONVERTERS_H_

#include <vector>

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"

namespace password_manager {

// Converts a StoredCredential to a PasswordForm.
// Passes by value to support efficient moves.
PasswordForm ToPasswordForm(const StoredCredential& cred);
PasswordForm ToPasswordForm(StoredCredential&& cred);

// Converts a PasswordForm to a StoredCredential.
// Passes by value to support efficient moves.
StoredCredential FromPasswordForm(PasswordForm form);

// Converts a vector of StoredCredentials to a vector of PasswordForms.
std::vector<PasswordForm> ToPasswordForms(
    std::vector<StoredCredential> credentials);

// Converts a vector of PasswordForms to a vector of StoredCredentials.
std::vector<StoredCredential> FromPasswordForms(
    std::vector<PasswordForm> forms);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_FORM_CONVERTERS_H_
